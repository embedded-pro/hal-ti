"""SPI master (`hal::tiva::SpiMaster` / `SynchronousSpiMaster`), decoded from a logic-analyzer capture.

Wiring set `harness`: CLK, FSS, MOSI and MISO on DIOs (the SSI pins are also PWM outputs). The AD3 SDK has no
verified SPI-slave mode, so the firmware master is observed with the logic analyzer: MISO is driven to a static
level by the AD3, or, with `--with loopback` and a MOSI-MISO jumper, only monitored (the firmware must then read
back what it sent).
"""

from __future__ import annotations

import pytest
from ad3_waveforms_bench import analysis
from ad3_waveforms_bench.terminal import FirmwareError


@pytest.fixture
def spi_cfg(board_cfg):
    return board_cfg.param("spi")


@pytest.fixture
def instance(spi_cfg):
    return spi_cfg["instances"][0]


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


@pytest.mark.ad3
@pytest.mark.matrix("spi.transfer")
@pytest.mark.board_params("miso_level", "spi.miso_levels")
@pytest.mark.constraint(valid=cs_only_async)
def test_transfer(fw, ad3, need, wiring, spi_cfg, instance, mode, baud, sync, cs, miso_level):
    dios = spi_dios(need, instance)
    loopback = prepare_miso(ad3, wiring, dios, miso_level)
    use_cs = cs == "fss"
    open_spi(fw, instance, use_cs, baud=baud, mode=mode, sync=sync)
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


@pytest.mark.ad3
@pytest.mark.matrix("spi.sessions")
def test_continued_session(fw, ad3, need, spi_cfg, instance, mode, sync):
    """`continue=1` keeps the session open for the next `spi.xfer`; the bytes of both arrive in order."""
    dios = spi_dios(need, instance)
    ad3.dio.drive(dios["miso"], 0)
    use_cs = not sync
    baud = spi_cfg.get("session_baud", 20000)
    open_spi(fw, instance, use_cs, baud=baud, mode=mode, sync=sync)
    duration = 0.2
    rate = min(ad3.logic.clock_hz, ad3.logic.buffer_size / duration)
    if rate < 4 * baud:
        pytest.skip("the session does not fit the logic analyzer buffer")
    trigger = (dios["cs"], "falling") if use_cs else (dios["clk"], "either")
    capture = ad3.logic.arm(rate, int(duration * rate), trigger=trigger, pretrigger=0.01)
    fw.spi.xfer(instance["index"], b"\x12\x34", continue_=True)
    fw.spi.xfer(instance["index"], b"\x56", continue_=False)
    result = capture.wait(timeout=3.0)
    frames = result.spi(dios["clk"], dios["mosi"], None, dios["cs"] if use_cs else None, mode)
    assert analysis.spi_join(frames)[0] == b"\x12\x34\x56"


@pytest.mark.ad3
@pytest.mark.board_params("sync", values=[0, 1])
def test_receive_only_and_largest_transfer(fw, ad3, need, wiring, spi_cfg, instance, sync):
    dios = spi_dios(need, instance)
    loopback = prepare_miso(ad3, wiring, dios, 0 if wiring.has("loopback") else 1)
    open_spi(fw, instance, not sync, baud=1000000, sync=sync)
    assert fw.spi.xfer(instance["index"], b"", rx=4) == (b"\x00" * 4 if loopback else b"\xff" * 4)
    size = spi_cfg["max_transfer"]
    payload = bytes((i * 11 + 5) & 0xFF for i in range(size))
    assert fw.spi.xfer(instance["index"], payload) == (payload if loopback else b"\xff" * size)
    assert fw.spi.xfer(instance["index"], payload[:8], rx=0) == b""
    assert fw.spi.xfer(instance["index"], payload[:2], rx=6) == (payload[:2] + b"\x00" * 4 if loopback else b"\xff" * 6)


def test_open_errors(fw, instance):
    sysclk = fw.system.info().sysclk
    pins = {"clk": instance["clk"], "mosi": instance["mosi"], "miso": instance["miso"]}
    cases = [
        ({**pins, "baud": sysclk // 2 + 1}, "range"),
        ({**pins, "baud": sysclk // 65024}, "range"),
        ({**pins, "mode": 4}, "range"),
        ({"clk": instance["clk"], "mosi": instance["mosi"]}, "usage"),
        ({**pins, "cs": instance["cs"], "sync": True}, "unsupported"),
    ]
    for options, reason in cases:
        with pytest.raises(FirmwareError) as error:
            fw.spi.open(instance["index"], **options)
        assert error.value.reason == reason, options
    fw.spi.open(instance["index"], **pins, baud=sysclk // 2)
