"""SPI master (`hal::tiva::SpiMaster` / `SynchronousSpiMaster`), decoded from a logic-analyzer capture.

Wiring set `spi`: CLK, FSS, MOSI and MISO on DIOs. MISO is driven to a static level by the AD3, or, with
`--with loopback` and a MOSI-MISO jumper, only monitored (the firmware must then read back what it sent).
"""

import statistics

import pytest

from hal_ti_validation import analysis

pytestmark = pytest.mark.ad3


def spi_dios(need, instance):
    return {key: need.dio(instance[key]) for key in ("clk", "cs", "mosi", "miso")}


def measured_baud(clk, rate):
    highs, lows = analysis.high_low_times(clk, rate)
    if not highs or not lows:
        return 0.0
    return 1 / (statistics.median(highs) + statistics.median(lows))


def transfer_and_capture(fw, ad3, instance, dios, payload, baud):
    duration = len(payload) * 8 / baud * 3 + 200e-6
    rate = min(ad3.logic.clock_hz, 20 * baud, ad3.logic.buffer_size / duration)
    if rate < 4 * baud:
        pytest.skip(f"{len(payload)} bytes at {baud} Hz do not fit the logic analyzer buffer")
    capture = ad3.logic.arm(rate, int(duration * rate), trigger=(dios["cs"], "falling"), pretrigger=0.05)
    received = fw.spi.xfer(instance["index"], payload)
    return received, capture.wait(timeout=2.0)


@pytest.mark.board_params("instance", "spi.instances")
@pytest.mark.board_params("mode", "spi.modes")
@pytest.mark.board_params("baud", "spi.bauds")
@pytest.mark.board_params("sync", "spi.sync")
@pytest.mark.board_params("miso_level", "spi.miso_levels")
def test_transfer(fw, ad3, need, board_cfg, wiring, instance, mode, baud, sync, miso_level):
    dios = spi_dios(need, instance)
    loopback = wiring.has("loopback")
    if loopback and miso_level:
        pytest.skip("MISO follows MOSI with the loopback jumper")
    if not loopback:
        ad3.dio.drive(dios["miso"], miso_level)
    fw.spi.open(
        instance["index"],
        clk=instance["clk"],
        mosi=instance["mosi"],
        miso=instance["miso"],
        cs=instance["cs"],
        baud=baud,
        mode=mode,
        sync=sync,
    )
    tolerance = board_cfg.param("spi.tolerance.baud")
    for text in board_cfg.param("spi.payloads"):
        payload = bytes.fromhex(text)
        expected_rx = payload if loopback else bytes([0xFF if miso_level else 0x00] * len(payload))
        received, capture = transfer_and_capture(fw, ad3, instance, dios, payload, baud)
        assert received == expected_rx
        frames = capture.spi(dios["clk"], dios["mosi"], dios["miso"], dios["cs"], mode)
        mosi, miso = analysis.spi_join(frames)
        assert mosi == payload, f"decoded MOSI {mosi.hex()} (frames: {len(frames)})"
        assert miso == expected_rx
        clk = capture.channel(dios["clk"])
        cpol, _ = analysis.spi_mode_bits(mode)
        assert analysis.clock_idle_level(clk, capture.channel(dios["cs"])) == cpol, "clock idle level (CPOL)"
        assert measured_baud(clk, capture.rate) == pytest.approx(baud, rel=tolerance)


@pytest.mark.board_params("instance", "spi.instances")
def test_receive_only(fw, ad3, need, wiring, instance):
    dios = spi_dios(need, instance)
    if wiring.has("loopback"):
        pytest.skip("needs the AD3 to drive MISO")
    ad3.dio.drive(dios["miso"], 1)
    fw.spi.open(instance["index"], clk=instance["clk"], mosi=instance["mosi"], miso=instance["miso"], cs=instance["cs"])
    assert fw.spi.xfer(instance["index"], b"", rx=4) == b"\xff" * 4


@pytest.mark.board_params("instance", "spi.instances")
def test_continued_transfer(fw, ad3, need, instance):
    dios = spi_dios(need, instance)
    ad3.dio.drive(dios["miso"], 0)
    fw.spi.open(instance["index"], clk=instance["clk"], mosi=instance["mosi"], miso=instance["miso"], cs=instance["cs"], mode=1)
    capture = ad3.logic.arm(4e6, min(ad3.logic.buffer_size, 40000), trigger=(dios["cs"], "falling"), pretrigger=0.01)
    fw.spi.xfer(instance["index"], b"\x12\x34", continue_=True)
    fw.spi.xfer(instance["index"], b"\x56", continue_=False)
    result = capture.wait(timeout=3.0)
    frames = result.spi(dios["clk"], dios["mosi"], None, dios["cs"], 1)
    assert analysis.spi_join(frames)[0] == b"\x12\x34\x56"
