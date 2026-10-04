"""GPIO (`hal::tiva::GpioPin`): levels, pulls, open drain, interrupts and timer-driven pulses.

Wiring set `bundle1`: the pins of `tests.gpio.loop_pins`/`output_pins` are pins of other peripherals, used here as
plain GPIO; a locked pin that does not fit in bundle1 is in `bundle2` (EK-TM4C1294XL), and a board
without a user LED on its headers has no `output_pins`.

Scenarios: features/gpio.feature.
"""

import statistics

import pytest
from ad3_waveforms_bench import analysis
from ad3_waveforms_bench.terminal import FirmwareError
from pytest_bdd import given, parsers, scenario, then, when


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


def expected_edges(edge, pulses):
    return {"rising": pulses, "falling": pulses, "both": 2 * pulses}[edge]


@pytest.mark.board_params("pin", "gpio.loop_pins")
@pytest.mark.board_params("drive", "gpio.drives")
@scenario("gpio.feature", "The pin drives the DIO with every drive strength")
def test_output_levels(pin, drive):
    pass


@pytest.mark.board_params("pin", "gpio.output_pins")
@scenario("gpio.feature", "The output pins drive the DIO")
def test_output_pins(pin):
    pass


@pytest.mark.board_params("pin", "gpio.loop_pins")
@pytest.mark.board_params("pull", values=["none", "up", "down"])
@scenario("gpio.feature", "The pin reads the level the DIO drives with every pull")
def test_input_follows_ad3(pin, pull):
    pass


@pytest.mark.board_params("pin", "gpio.locked_pins")
@scenario("gpio.feature", "A locked pin works as an output and as an input")
def test_locked_pins_usable(pin):
    pass


@pytest.mark.board_params("pin", "gpio.loop_pins")
@pytest.mark.board_params("pull", "gpio.pulls")
@scenario("gpio.feature", "The pull sets the idle level")
def test_pull_sets_idle_level(pin, pull):
    pass


@pytest.mark.board_params("pin", "gpio.loop_pins")
@scenario("gpio.feature", "An open-drain pin pulls low and, released, follows the external level")
def test_open_drain(pin):
    pass


# The ad3 marker without the @ad3 tag, which would also open the AD3 that this scenario does not use.
@pytest.mark.ad3
@scenario("gpio.feature", "Open drain refuses a pull")
def test_open_drain_rejects_pull():
    pass


@pytest.mark.board_params("pin", "gpio.loop_pins")
@pytest.mark.matrix("gpio.irq")
@scenario("gpio.feature", "The interrupt counts the edges of the pulses and stops counting when turned off")
def test_interrupt_counts(pin, edge, handler, pulses, frequency):
    pass


@pytest.mark.board_params("pin", "gpio.loop_pins")
@pytest.mark.board_params("period_ms", "gpio.pulse.periods_ms")
@scenario("gpio.feature", "Timer-driven pulses toggle the pin at the period")
def test_pulse_timing(pin, period_ms):
    pass


@given("the pin is wired to a DIO", target_fixture="dio")
def pin_wired(need, pin):
    return need.dio(pin)


@given("the DIO is released")
def dio_released(ad3, dio):
    ad3.dio.release(dio)


@given(parsers.parse('the pin is configured as an input with pull "{fixed_pull}"'))
def input_with_fixed_pull(fw, pin, fixed_pull):
    fw.gpio.cfg(pin, "in", pull=fixed_pull)


@given(parsers.parse('the interrupt on the edge is enabled with the handler, unless that fails with "{reason}" for the port of the pin'))
def interrupt_enabled(fw, pin, edge, handler, reason):
    try:
        fw.gpio.irq(pin, edge, type=handler)
    except FirmwareError as error:
        if error.reason == reason:
            pytest.skip(f"no interrupt support on the port of {pin}")
        raise


@given("the edge count of the pin is cleared")
def count_cleared(fw, pin):
    fw.gpio.count(pin, clear=True)


@given(
    parsers.parse("the pulse count, tolerance and jitter of the board file, the jitter {default_ms:g} ms unless set"),
    target_fixture="pulse_spec",
)
def pulse_spec(board_cfg, default_ms):
    return {
        "count": board_cfg.param("gpio.pulse.count"),
        "tolerance": board_cfg.param("gpio.pulse.tolerance"),
        "jitter": board_cfg.param("gpio.pulse.jitter_ms", default_ms) / 1000,
    }


@given("the pin is configured as an output")
def output_configured(fw, pin):
    fw.gpio.cfg(pin, "out")


@given(parsers.parse("the pin is set to {level:d}"))
@when(parsers.parse("the pin is set to {level:d}"))
def pin_set(fw, pin, level):
    fw.gpio.set(pin, level)


@when("the pin is released")
def pin_released(fw, pin):
    fw.gpio.release(pin)


@when("the pin is configured as an input with the pull")
def input_with_pull(fw, pin, pull):
    fw.gpio.cfg(pin, "in", pull=pull)


@when("the pin is configured as open drain")
def open_drain_configured(fw, pin):
    fw.gpio.cfg(pin, "od")


@when("the AD3 sends the pulses on the DIO at the frequency")
def pulses_sent(ad3, dio, pulses, frequency):
    ad3.pattern.pulses(dio, pulses, frequency)
    ad3.pattern.wait_done(timeout=pulses / frequency + 2)


@when(parsers.parse("the firmware waits {ms:d} ms"))
def firmware_waits(fw, ms):
    fw.system.delay(ms)


@when("the interrupt is turned off")
def interrupt_off(fw, pin):
    fw.gpio.irq(pin, "off")


@when(parsers.parse("the AD3 sends {number:d} pulses on the DIO at the frequency"))
def more_pulses_sent(ad3, dio, frequency, number):
    ad3.pattern.pulses(dio, number, frequency)
    ad3.pattern.wait_done(timeout=number / frequency + 2)


@when(
    "the pin pulses the pulse count of times at the period while the logic analyzer records the DIO from just before its first rising edge",
    target_fixture="recording",
)
def pulses_recorded(fw, ad3, dio, pin, period_ms, pulse_spec):
    count = pulse_spec["count"]
    duration = (count + 1) * period_ms / 1000
    rate = min(ad3.logic.clock_hz, ad3.logic.buffer_size / (duration * 1.2))
    capture = ad3.logic.arm(rate, int(duration * 1.2 * rate), trigger=(dio, "rising"), pretrigger=0.02)
    fw.gpio.pulse(pin, count, period_ms)
    return {"rate": rate, "bits": capture.wait(timeout=duration + 2).channel(dio)}


@then("the pin, configured as an output with the drive strength, drives the DIO to every level it is set to")
def drives_with_strength(fw, ad3, pin, dio, drive):
    check_output(fw, ad3, pin, dio, drive)


@then("the pin, configured as an output, drives the DIO to every level it is set to")
def drives(fw, ad3, pin, dio):
    check_output(fw, ad3, pin, dio)


@then("the pin, configured as an input with the pull, reads every level the DIO drives, and the DIO is released")
def reads_with_pull(fw, ad3, pin, dio, pull):
    check_input(fw, ad3, pin, dio, pull)


@then(parsers.parse('the pin, configured as an input with pull "{fixed_pull}", reads every level the DIO drives, and the DIO is released'))
def reads_with_fixed_pull(fw, ad3, pin, dio, fixed_pull):
    check_input(fw, ad3, pin, dio, fixed_pull)


@then("the pin and the DIO read high if the pull is up and low otherwise")
def idle_level(fw, ad3, pin, dio, pull):
    expected = 1 if pull == "up" else 0
    assert fw.gpio.get(pin) == expected
    assert ad3.dio.read(dio) == expected


@then("the DIO is pulled low")
def pulled_low(ad3, dio):
    assert ad3.dio.read(dio) == 0, "open drain must pull low"


@then("the released open-drain pin reads every level the DIO drives, and the DIO is released")
def open_drain_follows(fw, ad3, pin, dio):
    for level in (1, 0, 1):
        ad3.dio.drive(dio, level)
        assert fw.gpio.get(pin) == level, "released open-drain pin must follow the external level"
    ad3.dio.release(dio)


@then(parsers.parse('configuring the first of the loop pins as open drain with pull "{fixed_pull}" fails with "{reason}"'))
def open_drain_pull_refused(fw, board_cfg, fixed_pull, reason):
    pin = board_cfg.param("gpio.loop_pins")[0]
    with pytest.raises(FirmwareError) as error:
        fw.gpio.cfg(pin, "od", pull=fixed_pull)
    assert error.value.reason == reason


@then("the edge count is the number of pulses, twice that for both edges")
def edges_counted(fw, pin, edge, pulses):
    assert fw.gpio.count(pin) == expected_edges(edge, pulses)


@then("the edge count is still the number of the first pulses, twice that for both edges")
def counting_stopped(fw, pin, edge, pulses):
    assert fw.gpio.count(pin) == expected_edges(edge, pulses), "counting must stop after irq off"


@then("the DIO toggles the pulse count of times", target_fixture="toggles")
def toggle_count(recording, pulse_spec):
    count = pulse_spec["count"]
    found = analysis.edges(recording["bits"])
    assert len(found) == count, f"{len(found)} toggles instead of {count}"
    return found


@then("the intervals between the toggles average the period within the tolerance and none is off by more than the jitter, if there are any")
def toggle_intervals(recording, pulse_spec, toggles, period_ms):
    rate, tolerance, jitter = recording["rate"], pulse_spec["tolerance"], pulse_spec["jitter"]
    intervals = [(b.index - a.index) / rate for a, b in zip(toggles, toggles[1:])]
    if intervals:
        assert statistics.fmean(intervals) == pytest.approx(period_ms / 1000, rel=tolerance)
        worst = max(abs(interval - period_ms / 1000) for interval in intervals)
        assert worst <= jitter, f"toggle interval off by {worst * 1000:.3f} ms"
