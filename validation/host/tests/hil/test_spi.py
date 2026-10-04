"""SPI master (`hal::tiva::SpiMaster` / `SynchronousSpiMaster`), decoded from a logic-analyzer capture.

Wiring set `bundle1`: CLK, FSS, MOSI and MISO on DIOs (the SSI pins are also PWM outputs). The AD3 SDK has no
verified SPI-slave mode, so the firmware master is observed with the logic analyzer: MISO is driven to a static
level by the AD3, or, with `--with loopback` and a MOSI-MISO jumper, only monitored (the firmware must then read
back what it sent).

Scenarios: features/spi.feature.
"""

from __future__ import annotations

import pytest
from ad3_waveforms_bench import analysis
from ad3_waveforms_bench.terminal import FirmwareError
from pytest_bdd import given, parsers, scenario, then, when


@pytest.fixture
def spi_cfg(board_cfg):
    return board_cfg.param("spi")


@pytest.fixture
def instance(spi_cfg):
    return spi_cfg["instances"][0]


@pytest.fixture
def session_baud(spi_cfg):
    return spi_cfg.get("session_baud", 20000)


@pytest.fixture
def spi_pins(instance):
    return {"clk": instance["clk"], "mosi": instance["mosi"], "miso": instance["miso"]}


def spi_dios(need, instance):
    return {key: need.dio(instance[key]) for key in ("clk", "cs", "mosi", "miso")}


def cs_only_async(values):
    return not (values.get("sync") and values.get("cs") == "fss")


def measured_clock(clk, rate, baud):
    """Clock frequency over each burst of edges (a gap of more than 1.5 periods separates bursts)."""
    edges = [edge.index for edge in analysis.edges(clk)]
    gap = 1.5 * rate / baud
    bursts, current = [], edges[:1]
    for index in edges[1:]:
        if index - current[-1] > gap:
            bursts.append(current)
            current = [index]
        else:
            current.append(index)
    bursts.append(current)
    periods = sum((len(burst) - 1) / 2 for burst in bursts if len(burst) > 2)
    samples = sum(burst[-1] - burst[0] for burst in bursts if len(burst) > 2)
    return periods * rate / samples if samples else 0.0


def open_spi(fw, instance, use_cs, **options):
    fw.spi.open(
        instance["index"],
        clk=instance["clk"],
        mosi=instance["mosi"],
        miso=instance["miso"],
        cs=instance["cs"] if use_cs else None,
        **options,
    )


def expected_rx(payload, loopback, miso_level):
    return payload if loopback else bytes([0xFF if miso_level else 0x00] * len(payload))


def prepare_miso(ad3, wiring, dios, miso_level):
    loopback = wiring.has("loopback")
    if loopback and miso_level:
        pytest.skip("MISO follows MOSI with the loopback jumper")
    if not loopback:
        ad3.dio.drive(dios["miso"], miso_level)
    return loopback


def open_refused(fw, instance, options, reason):
    with pytest.raises(FirmwareError) as error:
        fw.spi.open(instance["index"], **options)
    assert error.value.reason == reason, options


@pytest.mark.matrix("spi.transfer")
@pytest.mark.board_params("miso_level", "spi.miso_levels")
@pytest.mark.constraint(valid=cs_only_async)
@scenario("spi.feature", "The master exchanges payloads in the mode at the baud rate")
def test_transfer(mode, baud, sync, cs, miso_level):
    pass


@pytest.mark.matrix("spi.sessions")
@scenario("spi.feature", "A continued session sends the bytes of both transfers in order")
def test_continued_session(mode, sync):
    pass


@pytest.mark.board_params("sync", values=[0, 1])
@scenario("spi.feature", "Receive-only, largest and uneven transfers return the MISO data")
def test_receive_only_and_largest_transfer(sync):
    pass


@scenario("spi.feature", "Invalid settings are refused and half the system clock opens")
def test_open_errors():
    pass


@given("the CLK, FSS, MOSI and MISO pins of the instance are wired", target_fixture="dios")
def pins_wired(need, instance):
    return spi_dios(need, instance)


@given(
    "the AD3 drives MISO to the MISO level, or with the loopback jumper MISO follows MOSI, which needs MISO level 0",
    target_fixture="loopback",
)
def miso_at_level(ad3, wiring, dios, miso_level):
    return prepare_miso(ad3, wiring, dios, miso_level)


@given("the AD3 drives MISO high, or with the loopback jumper MISO follows MOSI", target_fixture="loopback")
def miso_high_or_looped_back(ad3, wiring, dios):
    return prepare_miso(ad3, wiring, dios, 0 if wiring.has("loopback") else 1)


@given("the AD3 drives MISO low")
def miso_low(ad3, dios):
    ad3.dio.drive(dios["miso"], 0)


@given("the instance is open in the mode at the baud rate, synchronous or not, on its FSS pin if the chip select is fss")
def open_for_transfer(fw, instance, mode, baud, sync, cs):
    open_spi(fw, instance, cs == "fss", baud=baud, mode=mode, sync=sync)


@given("the instance is open in the mode at the session baud rate, synchronous or not, on its FSS pin unless synchronous")
def open_for_session(fw, instance, session_baud, mode, sync):
    open_spi(fw, instance, not sync, baud=session_baud, mode=mode, sync=sync)


@given(parsers.parse("the instance is open at {bit_rate:d} baud, synchronous or not, on its FSS pin unless synchronous"))
def open_at_bit_rate(fw, instance, sync, bit_rate):
    open_spi(fw, instance, not sync, baud=bit_rate, sync=sync)


@given(
    parsers.parse(
        "the logic analyzer records {duration:g} s, triggered on FSS falling or, when synchronous, on any clock edge, if it can sample at "
        "{oversampling:d} times the session baud rate"
    ),
    target_fixture="capture",
)
def session_recorded(ad3, dios, session_baud, sync, duration, oversampling):
    use_cs = not sync
    rate = min(ad3.logic.clock_hz, ad3.logic.buffer_size / duration)
    if rate < oversampling * session_baud:
        pytest.skip("the session does not fit the logic analyzer buffer")
    trigger = (dios["cs"], "falling") if use_cs else (dios["clk"], "either")
    return ad3.logic.arm(rate, int(duration * rate), trigger=trigger, pretrigger=0.01)


@given("the system clock the firmware reports", target_fixture="sysclk")
def system_clock(fw):
    return fw.system.info().sysclk


@when(parsers.parse('"{first}" is sent with continue=1 and then "{second}" with continue=0'))
def continued_transfers(fw, instance, first, second):
    fw.spi.xfer(instance["index"], bytes.fromhex(first), continue_=True)
    fw.spi.xfer(instance["index"], bytes.fromhex(second), continue_=False)


@then(
    "each of the payloads sent with spi.xfer returns its MISO data; where the logic analyzer can record it at 4 or more samples per "
    "clock period, triggered on FSS falling or else on any clock edge, MOSI decodes to the payload and MISO to its MISO data, the "
    "clock idles at the CPOL of the mode and runs at the baud rate within the baud tolerance, and FSS ends high"
)
def payloads_exchanged(fw, ad3, spi_cfg, instance, dios, loopback, mode, baud, cs, miso_level):
    use_cs = cs == "fss"
    cs_dio = dios["cs"] if use_cs else None
    for text in spi_cfg["payloads"]:
        payload = bytes.fromhex(text)
        wanted = expected_rx(payload, loopback, miso_level)
        duration = len(payload) * 8 / baud * 4 + 200e-6
        rate = min(ad3.logic.clock_hz, 20 * baud, ad3.logic.buffer_size / duration)
        decodable = rate >= 4 * baud
        trigger = (dios["cs"], "falling") if use_cs else (dios["clk"], "either")
        capture = ad3.logic.arm(rate, int(duration * rate), trigger=trigger, pretrigger=0.05) if decodable else None
        assert fw.spi.xfer(instance["index"], payload) == wanted
        if capture is None:
            continue
        result = capture.wait(timeout=2.0)
        frames = result.spi(dios["clk"], dios["mosi"], dios["miso"], cs_dio, mode)
        mosi, miso = analysis.spi_join(frames)
        assert mosi == payload, f"decoded MOSI {mosi.hex()} (frames: {len(frames)})"
        assert miso == wanted
        clk = result.channel(dios["clk"])
        cpol, _ = analysis.spi_mode_bits(mode)
        cs_bits = None if cs_dio is None else result.channel(cs_dio)
        assert analysis.clock_idle_level(clk, cs_bits) == cpol, "clock idle level (CPOL)"
        assert measured_clock(clk, result.rate, baud) == pytest.approx(baud, rel=spi_cfg["tolerance"]["baud"])
        if cs_bits is not None:
            assert cs_bits[-1] == 1, "FSS must be released after the transfer"


@then(parsers.parse('the recording decodes on MOSI to "{data}"'))
def session_decodes(dios, capture, mode, sync, data):
    use_cs = not sync
    result = capture.wait(timeout=3.0)
    frames = result.spi(dios["clk"], dios["mosi"], None, dios["cs"] if use_cs else None, mode)
    assert analysis.spi_join(frames)[0] == bytes.fromhex(data)


@then(parsers.parse("receiving {count:d} bytes without sending returns 00 bytes with the loopback jumper, else ff bytes"))
def receive_only(fw, instance, loopback, count):
    assert fw.spi.xfer(instance["index"], b"", rx=count) == (b"\x00" * count if loopback else b"\xff" * count)


@then("a payload of the largest transfer size returns itself with the loopback jumper, else ff bytes", target_fixture="largest")
def largest_transfer(fw, spi_cfg, instance, loopback):
    size = spi_cfg["max_transfer"]
    payload = bytes((i * 11 + 5) & 0xFF for i in range(size))
    assert fw.spi.xfer(instance["index"], payload) == (payload if loopback else b"\xff" * size)
    return payload


@then(parsers.parse("sending its first {sent:d} bytes while receiving {received:d} returns nothing"))
def send_without_receiving(fw, instance, largest, sent, received):
    assert fw.spi.xfer(instance["index"], largest[:sent], rx=received) == b""


@then(
    parsers.parse(
        "sending its first {sent:d} bytes while receiving {received:d} returns them followed by 00 bytes with the loopback jumper, "
        "else ff bytes"
    )
)
def send_and_receive_more(fw, instance, loopback, largest, sent, received):
    assert fw.spi.xfer(instance["index"], largest[:sent], rx=received) == (
        largest[:sent] + b"\x00" * (received - sent) if loopback else b"\xff" * received
    )


@then(
    parsers.parse(
        "opening the instance on its CLK, MOSI and MISO pins at the system clock over {divisor:d} plus {extra:d} baud fails with "
        '"{refusal}"'
    )
)
def open_above_fastest_refused(fw, instance, spi_pins, sysclk, divisor, extra, refusal):
    open_refused(fw, instance, {**spi_pins, "baud": sysclk // divisor + extra}, refusal)


@then(parsers.parse('opening the instance on its CLK, MOSI and MISO pins at the system clock over {divisor:d} baud fails with "{refusal}"'))
def open_below_slowest_refused(fw, instance, spi_pins, sysclk, divisor, refusal):
    open_refused(fw, instance, {**spi_pins, "baud": sysclk // divisor}, refusal)


@then(parsers.parse('opening the instance on its CLK, MOSI and MISO pins in mode {mode_number:d} fails with "{refusal}"'))
def open_bad_mode_refused(fw, instance, spi_pins, mode_number, refusal):
    open_refused(fw, instance, {**spi_pins, "mode": mode_number}, refusal)


@then(parsers.parse('opening the instance on its CLK and MOSI pins fails with "{refusal}"'))
def open_without_miso_refused(fw, instance, refusal):
    open_refused(fw, instance, {"clk": instance["clk"], "mosi": instance["mosi"]}, refusal)


@then(parsers.parse('opening the instance synchronously on its CLK, MOSI, MISO and FSS pins fails with "{refusal}"'))
def open_sync_with_fss_refused(fw, instance, spi_pins, refusal):
    open_refused(fw, instance, {**spi_pins, "cs": instance["cs"], "sync": True}, refusal)


@then(parsers.parse("the instance opens on its CLK, MOSI and MISO pins at the system clock over {divisor:d} baud"))
def opens_at_fastest(fw, instance, spi_pins, sysclk, divisor):
    fw.spi.open(instance["index"], **spi_pins, baud=sysclk // divisor)
