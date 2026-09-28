"""UART (`hal::tiva::Uart`, `UartWithDma`, `SynchronousUart`) against the AD3 protocol UART.

Wiring set `uart`: firmware TX and RX of `tests.uart.instances` on DIOs; `--with flow` adds RTS/CTS. The logic
analyzer also records the firmware TX line to decode the frames and measure the bit rate.
"""

from __future__ import annotations

import statistics
import time

import pytest
from ad3_waveforms_bench import analysis
from ad3_waveforms_bench.protocol import format_command
from ad3_waveforms_bench.terminal import FirmwareError

from hal_ti_validation import expect
from hal_ti_validation.firmware import settle


@pytest.fixture
def uart_cfg(board_cfg):
    return board_cfg.param("uart")


@pytest.fixture
def instance(uart_cfg):
    return uart_cfg["instances"][0]


def eight_n_one_for_sync(values):
    variant = values.get("variant")
    return variant != "sync" or (values.get("parity", "none") == "none" and values.get("stop", 1) == 1)


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


@pytest.mark.ad3
@pytest.mark.matrix("uart.transfer")
@pytest.mark.constraint(valid=eight_n_one_for_sync)
def test_transfer(fw, ad3, need, uart_cfg, instance, baud, parity, stop, variant):
    tx_dio = open_pair(fw, ad3, need, uart_cfg, instance, baud, parity, stop, variant)
    for text in uart_cfg["payloads"]:
        payload = bytes.fromhex(text)
        firmware_to_ad3(fw, ad3, instance["index"], payload, baud, parity, stop)
        ad3_to_firmware(fw, ad3, instance["index"], payload, baud, parity, stop)
    check_tx_waveform(fw, ad3, uart_cfg, instance["index"], tx_dio, baud, parity, stop)


@pytest.mark.ad3
@pytest.mark.matrix("uart.large")
def test_large_payloads(fw, ad3, need, board_cfg, uart_cfg, instance, baud, variant):
    open_pair(fw, ad3, need, uart_cfg, instance, baud, variant=variant)
    prefix = format_command("uart.send", instance["index"])
    size = min(uart_cfg["large_payload"], expect.max_hex_payload(board_cfg.terminal.max_command_length, prefix))
    firmware_to_ad3(fw, ad3, instance["index"], bytes(range(256))[:size], baud, "none", 1)
    inbound = uart_cfg["large_payload_to_firmware"]
    ad3_to_firmware(fw, ad3, instance["index"], bytes((i * 7) & 0xFF for i in range(inbound)), baud, "none", 1)


@pytest.mark.ad3
@pytest.mark.matrix("uart.large")
def test_full_duplex_stream(fw, ad3, need, uart_cfg, instance, baud, variant):
    """Both directions at once, round after round: nothing may be lost or reordered."""
    open_pair(fw, ad3, need, uart_cfg, instance, baud, variant=variant)
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


@pytest.mark.ad3
@pytest.mark.matrix("uart.flow")
def test_flow_control(fw, ad3, need, uart_cfg, flow, variant):
    """CTS deasserted (high) holds the firmware's transmitter; RTS is asserted (low) while it can receive."""
    instance = uart_cfg["flow_instance"]
    uses_rts, uses_cts = flow in ("rts", "rtscts"), flow in ("cts", "rtscts")
    rts = need.dio(role="uart_rts") if uses_rts else None
    cts = need.dio(role="uart_cts") if uses_cts else None
    baud = 115200
    if cts is not None:
        ad3.dio.drive(cts, 1)
    open_pair(
        fw,
        ad3,
        need,
        uart_cfg,
        instance,
        baud,
        variant=variant,
        rts=instance["rts"] if uses_rts else None,
        cts=instance["cts"] if uses_cts else None,
        flow=flow,
    )
    payload = bytes.fromhex("0123456789abcdef")
    if rts is not None:
        assert ad3.dio.read(rts) == 0, "RTS must be asserted (low) while the firmware can receive"
        ad3_to_firmware(fw, ad3, instance["index"], payload, baud, "none", 1)
    if cts is not None:
        pending = fw.terminal.begin(format_command("uart.send", instance["index"], payload), timeout=5.0)
        try:
            held = ad3.uart.read(len(payload), timeout=0.2)
            ad3.dio.drive(cts, 0)
            released = ad3.uart.read(len(payload), timeout=1.0)
        finally:
            response = settle(pending)
        assert held == b"", "data sent while CTS is deasserted"
        assert released == payload
        assert response is not None and response.ok, f"uart.send {response}"
    else:
        firmware_to_ad3(fw, ad3, instance["index"], payload, baud, "none", 1)


@pytest.mark.ad3
def test_reopen_with_other_settings(fw, ad3, need, uart_cfg, instance):
    for baud, parity, stop in ((9600, "even", 2), (460800, "none", 1), (921600, "odd", 1)):
        open_pair(fw, ad3, need, uart_cfg, instance, baud, parity, stop)
        firmware_to_ad3(fw, ad3, instance["index"], b"\x5a\xa5", baud, parity, stop)
        fw.uart.close(instance["index"])
        time.sleep(0.01)


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
def test_open_errors(fw, instance, options, reason):
    with pytest.raises(FirmwareError) as error:
        fw.uart.open(instance["index"], tx=instance["tx"], rx=instance["rx"], **options)
    assert error.value.reason == reason


def test_uart_without_pins_needs_default(fw, board_cfg, instance):
    """Only UART1 of the TM4C123 has default pins; every other instance needs tx and rx."""
    other = next(index for index in range(1, 8) if index not in (instance["index"], board_cfg.terminal.uart))
    with pytest.raises(FirmwareError) as error:
        fw.uart.open(other)
    assert error.value.reason == "usage"
    with pytest.raises(FirmwareError) as error:
        fw.uart.open(other, tx=instance["tx"])
    assert error.value.reason == "usage"
