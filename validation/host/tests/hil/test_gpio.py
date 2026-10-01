"""GPIO (`hal::tiva::GpioPin`): levels, pulls, open drain, interrupts and timer-driven pulses.

Wiring set `harness`: the pins of `tests.gpio.loop_pins`/`output_pins` are harness pins of other peripherals, used
here as plain GPIO; a locked pin outside the harness needs its own set (`locked` on the EK-TM4C1294XL).
"""

import statistics

import pytest
from ad3_waveforms_bench import analysis
from ad3_waveforms_bench.terminal import FirmwareError

pytestmark = pytest.mark.ad3


def check_output(fw, ad3, pin, dio, drive=None):
    fw.gpio.cfg(pin, "out", drive=drive)
    for level in (0, 1, 0, 1, 0):
        fw.gpio.set(pin, level)
        assert ad3.dio.read(dio) == level, f"{pin} set to {level}"


def check_input(fw, ad3, pin, dio, pull="none"):
    fw.gpio.cfg(pin, "in", pull=pull)
    for level in (1, 0, 1, 0):
        ad3.dio.drive(dio, level)
        assert fw.gpio.get(pin) == level, f"DIO{dio} driven {level}"
    ad3.dio.release(dio)


@pytest.mark.board_params("pin", "gpio.loop_pins")
@pytest.mark.board_params("drive", "gpio.drives")
def test_output_levels(fw, ad3, need, pin, drive):
    check_output(fw, ad3, pin, need.dio(pin), drive)


@pytest.mark.board_params("pin", "gpio.output_pins")
def test_output_pins(fw, ad3, need, pin):
    check_output(fw, ad3, pin, need.dio(pin))


@pytest.mark.board_params("pin", "gpio.loop_pins")
@pytest.mark.board_params("pull", values=["none", "up", "down"])
def test_input_follows_ad3(fw, ad3, need, pin, pull):
    check_input(fw, ad3, pin, need.dio(pin), pull)


@pytest.mark.board_params("pin", "gpio.locked_pins")
def test_locked_pins_usable(fw, ad3, need, pin):
    dio = need.dio(pin)
    check_output(fw, ad3, pin, dio)
    fw.gpio.release(pin)
    check_input(fw, ad3, pin, dio, "up")


@pytest.mark.board_params("pin", "gpio.loop_pins")
@pytest.mark.board_params("pull", "gpio.pulls")
def test_pull_sets_idle_level(fw, ad3, need, pin, pull):
    dio = need.dio(pin)
    ad3.dio.release(dio)
    fw.gpio.cfg(pin, "in", pull=pull)
    expected = 1 if pull == "up" else 0
    assert fw.gpio.get(pin) == expected
    assert ad3.dio.read(dio) == expected


@pytest.mark.board_params("pin", "gpio.loop_pins")
def test_open_drain(fw, ad3, need, pin):
    dio = need.dio(pin)
    ad3.dio.release(dio)
    fw.gpio.cfg(pin, "od")
    fw.gpio.set(pin, 0)
    assert ad3.dio.read(dio) == 0, "open drain must pull low"
    fw.gpio.set(pin, 1)
    for level in (1, 0, 1):
        ad3.dio.drive(dio, level)
        assert fw.gpio.get(pin) == level, "released open-drain pin must follow the external level"
    ad3.dio.release(dio)


def test_open_drain_rejects_pull(fw, board_cfg):
    pin = board_cfg.param("gpio.loop_pins")[0]
    with pytest.raises(FirmwareError) as error:
        fw.gpio.cfg(pin, "od", pull="up")
    assert error.value.reason == "usage"


def expected_edges(edge, pulses):
    return {"rising": pulses, "falling": pulses, "both": 2 * pulses}[edge]


@pytest.mark.board_params("pin", "gpio.loop_pins")
@pytest.mark.matrix("gpio.irq")
def test_interrupt_counts(fw, ad3, need, pin, edge, handler, pulses, frequency):
    dio = need.dio(pin)
    ad3.dio.release(dio)
    fw.gpio.cfg(pin, "in", pull="down")
    try:
        fw.gpio.irq(pin, edge, type=handler)
    except FirmwareError as error:
        if error.reason == "unsupported":
            pytest.skip(f"no interrupt support on the port of {pin}")
        raise
    fw.gpio.count(pin, clear=True)
    ad3.pattern.pulses(dio, pulses, frequency)
    ad3.pattern.wait_done(timeout=pulses / frequency + 2)
    fw.system.delay(10)
    assert fw.gpio.count(pin) == expected_edges(edge, pulses)
    fw.gpio.irq(pin, "off")
    ad3.pattern.pulses(dio, 3, frequency)
    ad3.pattern.wait_done(timeout=3 / frequency + 2)
    assert fw.gpio.count(pin) == expected_edges(edge, pulses), "counting must stop after irq off"


@pytest.mark.board_params("pin", "gpio.loop_pins")
@pytest.mark.board_params("period_ms", "gpio.pulse.periods_ms")
def test_pulse_timing(fw, ad3, need, board_cfg, pin, period_ms):
    dio = need.dio(pin)
    count = board_cfg.param("gpio.pulse.count")
    tolerance = board_cfg.param("gpio.pulse.tolerance")
    jitter = board_cfg.param("gpio.pulse.jitter_ms", 0.5) / 1000
    fw.gpio.cfg(pin, "out")
    fw.gpio.set(pin, 0)
    duration = (count + 1) * period_ms / 1000
    rate = min(ad3.logic.clock_hz, ad3.logic.buffer_size / (duration * 1.2))
    capture = ad3.logic.arm(rate, int(duration * 1.2 * rate), trigger=(dio, "rising"), pretrigger=0.02)
    fw.gpio.pulse(pin, count, period_ms)
    bits = capture.wait(timeout=duration + 2).channel(dio)
    found = analysis.edges(bits)
    assert len(found) == count, f"{len(found)} toggles instead of {count}"
    intervals = [(b.index - a.index) / rate for a, b in zip(found, found[1:])]
    if intervals:
        assert statistics.fmean(intervals) == pytest.approx(period_ms / 1000, rel=tolerance)
        worst = max(abs(interval - period_ms / 1000) for interval in intervals)
        assert worst <= jitter, f"toggle interval off by {worst * 1000:.3f} ms"
