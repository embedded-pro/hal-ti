"""UART (`hal::tiva::Uart`, `UartWithDma`, `SynchronousUart`) against the AD3 protocol UART.

Wiring set `bundle1`: firmware TX, RX, RTS and CTS of `tests.uart` on DIOs (`bundle2` for RTS/CTS on the
EK-TM4C1294XL). The logic
analyzer also records the firmware TX line to decode the frames and measure the bit rate.

Scenarios: features/uart.feature.
"""

from __future__ import annotations

import statistics
import time

import pytest
from ad3_waveforms_bench import analysis
from ad3_waveforms_bench.protocol import format_command
from ad3_waveforms_bench.terminal import FirmwareError
from pytest_bdd import given, parsers, scenario, then

from hal_ti_validation import expect
from hal_ti_validation.firmware import settle


@pytest.fixture
def uart_cfg(board_cfg):
    return board_cfg.param("uart")


@pytest.fixture
def instance(uart_cfg):
    return uart_cfg["instances"][0]


@pytest.fixture
def flow_instance(uart_cfg):
    return uart_cfg["flow_instance"]


def eight_n_one_for_sync(values):
    variant = values.get("variant")
    return variant != "sync" or (values.get("parity", "none") == "none" and values.get("stop", 1) == 1)


def flow_lines(flow):
    return flow in ("rts", "rtscts"), flow in ("cts", "rtscts")


def open_pair(fw, ad3, need, uart_cfg, instance, baud, parity="none", stop=1, variant="interrupt", **extra):
    ad3_rx = need.dio(instance["tx"])
    ad3_tx = need.dio(instance["rx"])
    driver = uart_cfg["variants"][variant]
    fw.uart.open(
        instance["index"],
        tx=instance["tx"],
        rx=instance["rx"],
        baud=baud,
        parity=parity,
        stop=stop,
        dma=driver["dma"],
        sync=driver["sync"],
        **extra,
    )
    ad3.uart.configure(tx=ad3_tx, rx=ad3_rx, baud=baud, parity=parity, stop=stop)
    ad3.uart.flush()
    fw.uart.recv(instance["index"])
    return ad3_rx


def firmware_to_ad3(fw, ad3, index, payload, baud, parity, stop):
    ad3.uart.flush()
    fw.uart.send(index, payload, cmd_timeout=2.0 + expect.uart_transfer_time(len(payload), baud, parity, stop))
    received = ad3.uart.read(len(payload), timeout=expect.uart_transfer_time(len(payload), baud, parity, stop) + 1.0)
    assert received == payload
    assert ad3.uart.parity_errors == 0


def ad3_to_firmware(fw, ad3, index, payload, baud, parity, stop):
    fw.uart.recv(index)
    ad3.uart.write(payload)
    wait_ms = min(10000, int(expect.uart_transfer_time(len(payload), baud, parity, stop) * 1000) + 500)
    assert fw.uart.recv(index, timeout=wait_ms, len=len(payload)) == payload


def measured_bit_rate(bits, starts, rate, baud):
    """Bit rate from each frame's start-bit edge to the edge that starts data bit 7 (8 bit times for 0x55)."""
    per_bit = rate / baud
    edges = [edge.index for edge in analysis.edges(bits)]
    times = []
    for start in starts:
        target = start + 8 * per_bit
        closest = min(edges, key=lambda index: abs(index - target))
        if abs(closest - target) < per_bit / 2:
            times.append((closest - start) / 8 / rate)
    assert times, "no complete frames to measure"
    return 1 / statistics.fmean(times)


def check_tx_waveform(fw, ad3, uart_cfg, index, dio, baud, parity, stop):
    """Decode the firmware TX line: the bytes, parity and stop bits, and the bit rate."""
    payload = b"\x55" * 16
    duration = expect.uart_transfer_time(len(payload), baud, parity, stop) * 1.5 + 2e-3
    rate = min(ad3.logic.clock_hz, ad3.logic.buffer_size / duration)
    if rate < 16 * baud:
        pytest.skip(f"{baud} baud does not fit the logic analyzer buffer at 16 samples per bit")
    capture = ad3.logic.arm(rate, int(duration * rate), trigger=(dio, "falling"), pretrigger=0.01)
    fw.uart.send(index, payload)
    result = capture.wait(timeout=duration + 2.0)
    decoded = result.uart(dio, baud, parity, stop)
    assert bytes(byte.value for byte in decoded) == payload
    assert not any(byte.parity_error or byte.framing_error for byte in decoded), "parity or framing error on the wire"
    measured = measured_bit_rate(result.channel(dio), [byte.start_index for byte in decoded], result.rate, baud)
    assert measured == pytest.approx(baud, rel=uart_cfg["bit_rate_tolerance"])


@pytest.mark.matrix("uart.transfer")
@pytest.mark.constraint(valid=eight_n_one_for_sync)
@scenario("uart.feature", "Payloads cross both ways and the TX line runs at the baud rate")
def test_transfer(baud, parity, stop, variant):
    pass


@pytest.mark.matrix("uart.large")
@scenario("uart.feature", "The largest payloads cross both ways")
def test_large_payloads(baud, variant):
    pass


@pytest.mark.matrix("uart.large")
@scenario("uart.feature", "Both directions stream at once without loss or reordering")
def test_full_duplex_stream(baud, variant):
    pass


@pytest.mark.matrix("uart.flow")
@scenario("uart.feature", "CTS holds the transmitter and RTS is asserted while the firmware can receive")
def test_flow_control(flow, variant):
    pass


@scenario("uart.feature", "The instance reopens with other settings")
def test_reopen_with_other_settings():
    pass


@pytest.mark.board_params(
    "options,reason",
    values=[
        [{"baud": 1000}, "usage"],
        [{"parity": "mark"}, "usage"],
        [{"dma": 1, "sync": 1}, "usage"],
        [{"sync": 1, "parity": "even"}, "unsupported"],
        [{"sync": 1, "stop": 2}, "unsupported"],
        [{"flow": "rts"}, "usage"],
        [{"flow": "cts"}, "usage"],
    ],
)
@scenario("uart.feature", "Invalid options are refused with their reason")
def test_open_errors(options, reason):
    pass


@scenario("uart.feature", "An instance without default pins needs TX and RX")
def test_uart_without_pins_needs_default():
    pass


@given("the instance is open against the AD3 at the baud rate with the parity, stop bits and variant", target_fixture="tx_dio")
def open_with_settings(fw, ad3, need, uart_cfg, instance, baud, parity, stop, variant):
    return open_pair(fw, ad3, need, uart_cfg, instance, baud, parity, stop, variant)


@given("the instance is open against the AD3 at the baud rate with the variant")
def open_with_variant(fw, ad3, need, uart_cfg, instance, baud, variant):
    open_pair(fw, ad3, need, uart_cfg, instance, baud, variant=variant)


@given("the RTS and CTS pins of the flow instance that the flow control uses are wired", target_fixture="flow_dios")
def flow_pins_wired(need, flow_instance, flow):
    uses_rts, uses_cts = flow_lines(flow)
    rts = need.dio(flow_instance["rts"]) if uses_rts else None
    cts = need.dio(flow_instance["cts"]) if uses_cts else None
    return {"rts": rts, "cts": cts}


@given("the AD3 drives CTS high if the flow control uses it")
def cts_deasserted(ad3, flow_dios):
    if flow_dios["cts"] is not None:
        ad3.dio.drive(flow_dios["cts"], 1)


@given(
    parsers.parse(
        "the flow instance is open against the AD3 at {baud_rate:d} baud with the variant and the flow control on the pins it uses"
    ),
    target_fixture="flow_baud",
)
def open_with_flow_control(fw, ad3, need, uart_cfg, flow_instance, flow, variant, baud_rate):
    uses_rts, uses_cts = flow_lines(flow)
    open_pair(
        fw,
        ad3,
        need,
        uart_cfg,
        flow_instance,
        baud_rate,
        variant=variant,
        rts=flow_instance["rts"] if uses_rts else None,
        cts=flow_instance["cts"] if uses_cts else None,
        flow=flow,
    )
    return baud_rate


@given(
    parsers.parse("the first of UART {first:d} to {last:d} that is neither the instance nor the terminal UART"), target_fixture="other_uart"
)
def pick_other_uart(board_cfg, instance, first, last):
    return next(index for index in range(first, last + 1) if index not in (instance["index"], board_cfg.terminal.uart))


@then("each of the payloads goes from the firmware to the AD3 and then from the AD3 to the firmware")
def payloads_cross_both_ways(fw, ad3, uart_cfg, instance, baud, parity, stop):
    for text in uart_cfg["payloads"]:
        payload = bytes.fromhex(text)
        firmware_to_ad3(fw, ad3, instance["index"], payload, baud, parity, stop)
        ad3_to_firmware(fw, ad3, instance["index"], payload, baud, parity, stop)


@then(
    "16 bytes 55 sent by the firmware decode from its TX line without parity or framing errors, at the baud rate within the bit rate "
    "tolerance, unless the logic analyzer cannot record them at 16 samples per bit"
)
def tx_line_decodes(fw, ad3, uart_cfg, instance, tx_dio, baud, parity, stop):
    check_tx_waveform(fw, ad3, uart_cfg, instance["index"], tx_dio, baud, parity, stop)


@then(
    "a payload counting up from 00, as long as one uart.send command line can carry and at most the large payload size, goes from "
    "the firmware to the AD3"
)
def largest_to_ad3(fw, ad3, board_cfg, uart_cfg, instance, baud):
    prefix = format_command("uart.send", instance["index"])
    size = min(uart_cfg["large_payload"], expect.max_hex_payload(board_cfg.terminal.max_command_length, prefix))
    firmware_to_ad3(fw, ad3, instance["index"], bytes(range(256))[:size], baud, "none", 1)


@then("a payload of the large payload size to the firmware, counting in steps of 7, goes from the AD3 to the firmware")
def large_to_firmware(fw, ad3, uart_cfg, instance, baud):
    inbound = uart_cfg["large_payload_to_firmware"]
    ad3_to_firmware(fw, ad3, instance["index"], bytes((i * 7) & 0xFF for i in range(inbound)), baud, "none", 1)


@then(
    "in each of the stream rounds, while the firmware sends a block of the stream size the AD3 sends another one, uart.send succeeds "
    "and each block arrives unchanged at the other end"
)
def stream_rounds_cross(fw, ad3, uart_cfg, instance, baud):
    size = uart_cfg["stream_size"]
    transfer = expect.uart_transfer_time(size, baud)
    for round_number in range(uart_cfg["stream_rounds"]):
        outbound = bytes((round_number * 31 + i) & 0xFF for i in range(size))
        inbound = bytes((round_number * 17 + 3 * i) & 0xFF for i in range(size))
        pending = fw.terminal.begin(format_command("uart.send", instance["index"], outbound), timeout=2.0 + transfer)
        try:
            ad3.uart.write(inbound)
            received = ad3.uart.read(size, timeout=transfer + 1.0)
        finally:
            response = settle(pending)
        assert response is not None and response.ok, f"round {round_number}: uart.send {response}"
        assert received == outbound, f"round {round_number}: firmware to AD3"
        assert fw.uart.recv(instance["index"], timeout=int(transfer * 1000) + 500, len=size) == inbound, (
            f"round {round_number}: AD3 to firmware"
        )


@then(parsers.parse('RTS reads low and "{data}" goes from the AD3 to the firmware, if the flow control uses RTS'))
def rts_asserted(fw, ad3, flow_instance, flow_dios, flow_baud, data):
    if flow_dios["rts"] is not None:
        assert ad3.dio.read(flow_dios["rts"]) == 0, "RTS must be asserted (low) while the firmware can receive"
        ad3_to_firmware(fw, ad3, flow_instance["index"], bytes.fromhex(data), flow_baud, "none", 1)


@then(
    parsers.parse(
        '"{data}" sent by the firmware does not reach the AD3 within {hold_s:g} s, reaches it within {release_s:g} s once the AD3 '
        "drives CTS low, and uart.send succeeds, if the flow control uses CTS"
    )
)
def cts_holds_transmitter(fw, ad3, flow_instance, flow_dios, data, hold_s, release_s):
    cts = flow_dios["cts"]
    if cts is not None:
        payload = bytes.fromhex(data)
        pending = fw.terminal.begin(format_command("uart.send", flow_instance["index"], payload), timeout=5.0)
        try:
            held = ad3.uart.read(len(payload), timeout=hold_s)
            ad3.dio.drive(cts, 0)
            released = ad3.uart.read(len(payload), timeout=release_s)
        finally:
            response = settle(pending)
        assert held == b"", "data sent while CTS is deasserted"
        assert released == payload
        assert response is not None and response.ok, f"uart.send {response}"


@then(parsers.parse('"{data}" goes from the firmware to the AD3, if the flow control does not use CTS'))
def sends_without_cts(fw, ad3, flow_instance, flow_dios, flow_baud, data):
    if flow_dios["cts"] is None:
        firmware_to_ad3(fw, ad3, flow_instance["index"], bytes.fromhex(data), flow_baud, "none", 1)


@then(
    parsers.parse(
        'with each of these settings in turn, the instance is opened against the AD3, "{data}" goes from the firmware to the AD3, '
        "the instance is closed and {pause_ms:d} ms pass"
    )
)
def reopens_with_settings(fw, ad3, need, uart_cfg, instance, datatable, data, pause_ms):
    header, *rows = datatable
    for row in rows:
        settings = dict(zip(header, row))
        baud, parity, stop = int(settings["baud"]), settings["parity"], int(settings["stop bits"])
        open_pair(fw, ad3, need, uart_cfg, instance, baud, parity, stop)
        firmware_to_ad3(fw, ad3, instance["index"], bytes.fromhex(data), baud, parity, stop)
        fw.uart.close(instance["index"])
        time.sleep(pause_ms / 1000)


@then("opening the instance on its TX and RX pins with the options fails with the reason")
def open_options_refused(fw, instance, options, reason):
    with pytest.raises(FirmwareError) as error:
        fw.uart.open(instance["index"], tx=instance["tx"], rx=instance["rx"], **options)
    assert error.value.reason == reason


@then(parsers.parse('opening it without pins fails with "{refusal}"'))
def open_without_pins_refused(fw, other_uart, refusal):
    with pytest.raises(FirmwareError) as error:
        fw.uart.open(other_uart)
    assert error.value.reason == refusal


@then(parsers.parse('opening it on the TX pin of the instance only fails with "{refusal}"'))
def open_tx_only_refused(fw, instance, other_uart, refusal):
    with pytest.raises(FirmwareError) as error:
        fw.uart.open(other_uart, tx=instance["tx"])
    assert error.value.reason == refusal
