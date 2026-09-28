"""PWM (`hal::tiva::Pwm` / `SynchronousPwm`): frequency, duty, dead band, alignment, interrupts, fault path.

Wiring set `pwm`: the A/B outputs of `tests.pwm.channels` on DIOs (and on TM4C129 W1 on the fault input).
"""

import statistics

import pytest

from hal_ti_validation import analysis, expect
from hal_ti_validation.terminal import FirmwareError

pytestmark = pytest.mark.ad3


@pytest.fixture
def pwm(board_cfg):
    return board_cfg.param("pwm")


@pytest.fixture
def channel_dios(pwm, need):
    return [(need.dio(channel["a"]), need.dio(channel["b"])) for channel in pwm["channels"]]


@pytest.fixture
def sysclk(fw):
    return fw.system.info().sysclk


def open_channels(fw, pwm, count, **options):
    channels = pwm["channels"][:count]
    return fw.pwm.open(
        pwm["module"],
        gens=[channel["gen"] for channel in channels],
        pins=[(channel["a"], channel["b"]) for channel in channels],
        **options,
    )


def record(ad3, pwm, frequency, trigger_dio=None, periods=None):
    periods = periods or pwm["capture_periods"]
    trigger = None if trigger_dio is None else (trigger_dio, "rising")
    return ad3.logic.record_for(periods / frequency, trigger=trigger, timeout=periods / frequency + 2)


def tolerance(pwm, key):
    return pwm["tolerance"][key]


@pytest.mark.board_params("frequency", "pwm.frequencies_hz")
@pytest.mark.board_params("duty", "pwm.duties")
@pytest.mark.board_params("mode", "pwm.modes")
@pytest.mark.board_params("sync", "pwm.sync")
def test_frequency_and_duty(fw, ad3, pwm, channel_dios, frequency, duty, mode, sync):
    a, _ = channel_dios[0]
    try:
        pwmclk = open_channels(fw, pwm, 1, freq=frequency, mode=mode, dead="off", sync=sync)
    except FirmwareError as error:
        if error.reason == "range":
            pytest.skip(f"{frequency} Hz does not fit the generator in {mode} mode")
        raise
    fw.pwm.duty(pwm["module"], duty)
    capture = record(ad3, pwm, frequency, a)
    expected = expect.pwm_frequency(pwmclk, frequency, mode)
    assert capture.frequency(a) == pytest.approx(expected, rel=tolerance(pwm, "frequency"))
    assert capture.duty(a) * 100 == pytest.approx(duty, abs=tolerance(pwm, "duty") + 100 * 2 * frequency / capture.rate)


@pytest.mark.board_params("divisor", "pwm.divisors")
@pytest.mark.board_params("frequency", "pwm.frequencies_hz")
def test_divisors(fw, ad3, pwm, channel_dios, sysclk, divisor, frequency):
    a, _ = channel_dios[0]
    pwmclk = sysclk / divisor
    fits = expect.pwm_fits(pwmclk, frequency, "center")
    try:
        reported = open_channels(fw, pwm, 1, freq=frequency, mode="center", div=divisor, dead="off")
    except FirmwareError as error:
        assert error.reason == "range" and not fits, f"ERR {error.reason} although LOAD fits"
        return
    assert fits, "the firmware accepted a frequency whose LOAD does not fit 16 bits"
    assert reported == pytest.approx(pwmclk)
    fw.pwm.duty(pwm["module"], 50)
    capture = record(ad3, pwm, frequency, a)
    assert capture.frequency(a) == pytest.approx(expect.pwm_frequency(reported, frequency, "center"), rel=tolerance(pwm, "frequency"))


def check_complementary(capture, a, b, frequency, dead_s, pwmclk, pwm):
    rate = capture.rate
    bits_a, bits_b = capture.channels(a, b)
    assert analysis.overlap_samples(bits_a, bits_b) == 0, "A and B are active together (shoot-through)"
    dead = analysis.dead_times_split(bits_a, bits_b, rate)
    allowed = tolerance(pwm, "dead_ticks") / pwmclk + 2 / rate
    for name, times in (("A off -> B on", dead.a_off_to_b_on), ("B off -> A on", dead.b_off_to_a_on)):
        assert times, f"no {name} transitions captured"
        assert statistics.median(times) == pytest.approx(dead_s, abs=allowed), name
    period = 1 / frequency
    high_a = statistics.median(analysis.high_low_times(bits_a, rate)[0])
    high_b = statistics.median(analysis.high_low_times(bits_b, rate)[0])
    assert high_a + high_b + 2 * dead_s == pytest.approx(period, abs=2 * allowed + 4 / rate)


@pytest.mark.board_params("dead_ns", "pwm.dead_times_ns")
@pytest.mark.board_params("mode", "pwm.modes")
@pytest.mark.board_params("sync", "pwm.sync")
def test_dead_time(fw, ad3, pwm, channel_dios, dead_ns, mode, sync):
    a, b = channel_dios[0]
    frequency = pwm["efoc"]["frequency_hz"]
    pwmclk = open_channels(fw, pwm, 1, freq=frequency, mode=mode, dead=dead_ns, sync=sync)
    fw.pwm.duty(pwm["module"], 40)
    capture = record(ad3, pwm, frequency, a)
    check_complementary(capture, a, b, frequency, expect.pwm_dead_time(dead_ns, pwmclk), pwmclk, pwm)


@pytest.mark.parametrize(("inva", "invb"), [(1, 0), (0, 1), (1, 1)])
def test_inversion(fw, ad3, pwm, channel_dios, inva, invb):
    a, b = channel_dios[0]
    frequency = pwm["efoc"]["frequency_hz"]
    open_channels(fw, pwm, 1, freq=frequency, mode="center", dead=pwm["efoc"]["dead_ns"])
    fw.pwm.duty(pwm["module"], 30)
    normal = record(ad3, pwm, frequency)
    fw.pwm.close(pwm["module"])
    open_channels(fw, pwm, 1, freq=frequency, mode="center", dead=pwm["efoc"]["dead_ns"], inva=inva, invb=invb)
    fw.pwm.duty(pwm["module"], 30)
    inverted = record(ad3, pwm, frequency)
    for dio, flag in ((a, inva), (b, invb)):
        expected = 1 - normal.duty(dio) if flag else normal.duty(dio)
        assert inverted.duty(dio) == pytest.approx(expected, abs=tolerance(pwm, "duty") / 100 + 0.01)


@pytest.mark.board_params("sync", "pwm.sync")
def test_efoc_default_configuration(fw, ad3, pwm, channel_dios, sysclk, sync):
    efoc = pwm["efoc"]
    pwmclk = fw.pwm.open(pwm["module"], sync=sync)
    assert pwmclk == pytest.approx(sysclk / efoc["divisor"])
    fw.pwm.duty(pwm["module"], 50, 50, 50)
    capture = record(ad3, pwm, efoc["frequency_hz"], channel_dios[0][0])
    expected = expect.pwm_frequency(pwmclk, efoc["frequency_hz"], efoc["mode"])
    dead = expect.pwm_dead_time(efoc["dead_ns"], pwmclk)
    for a, b in channel_dios:
        assert capture.frequency(a) == pytest.approx(expected, rel=tolerance(pwm, "frequency"))
        check_complementary(capture, a, b, efoc["frequency_hz"], dead, pwmclk, pwm)
    check_aligned(capture, [a for a, _ in channel_dios], "rising", tolerance(pwm, "alignment_s"))


def check_aligned(capture, dios, feature, allowed):
    def times(dio):
        bits = capture.channel(dio)
        if feature == "rising":
            return analysis.rising_times(bits, capture.rate)
        if feature == "falling":
            return analysis.falling_times(bits, capture.rate)
        return analysis.pulse_centers(bits, capture.rate)

    reference = times(dios[0])[1:-1]
    assert reference, "no complete pulses captured"
    for dio in dios[1:]:
        offsets = analysis.nearest_offsets(reference, times(dio))
        worst = max(abs(offset) for offset in offsets)
        assert worst <= allowed + 2 / capture.rate, f"DIO{dio} {feature} edges off by {worst * 1e9:.0f} ns"


@pytest.mark.board_params("mode", "pwm.modes")
@pytest.mark.parametrize("update", ["local", "global"])
def test_generators_are_synchronised(fw, ad3, pwm, channel_dios, mode, update):
    frequency = pwm["efoc"]["frequency_hz"]
    open_channels(fw, pwm, len(channel_dios), freq=frequency, mode=mode, dead="off", update=update)
    fw.pwm.duty(pwm["module"], *([50] * len(channel_dios)))
    capture = record(ad3, pwm, frequency, channel_dios[0][0])
    check_aligned(capture, [a for a, _ in channel_dios], "rising", tolerance(pwm, "alignment_s"))


@pytest.mark.board_params("mode", "pwm.modes")
def test_alignment_with_different_duties(fw, ad3, pwm, channel_dios, mode):
    frequency = pwm["efoc"]["frequency_hz"]
    duties = [20, 50, 80][: len(channel_dios)]
    open_channels(fw, pwm, len(channel_dios), freq=frequency, mode=mode, dead="off", update="global")
    fw.pwm.duty(pwm["module"], *duties)
    capture = record(ad3, pwm, frequency, channel_dios[0][0])
    dios = [a for a, _ in channel_dios]
    allowed = tolerance(pwm, "alignment_s")
    if mode == "center":
        check_aligned(capture, dios, "center", allowed)
    else:
        try:
            check_aligned(capture, dios, "rising", allowed)
        except AssertionError:
            check_aligned(capture, dios, "falling", allowed)
    for dio, duty in zip(dios, duties):
        assert capture.duty(dio) * 100 == pytest.approx(duty, abs=tolerance(pwm, "duty") + 1)


@pytest.mark.parametrize("duty", [0, 100])
def test_duty_extremes(fw, ad3, pwm, channel_dios, duty):
    efoc = pwm["efoc"]
    fw.pwm.open(pwm["module"])
    fw.pwm.duty(pwm["module"], *([duty] * len(channel_dios)))
    capture = record(ad3, pwm, efoc["frequency_hz"])
    for a, b in channel_dios:
        assert capture.edge_count(a) == 0 and capture.edge_count(b) == 0, "outputs must be static"
        assert capture.channel(a)[0] == (1 if duty == 100 else 0)
        assert capture.channel(b)[0] == (0 if duty == 100 else 1)


def test_frequency_change_and_stop(fw, ad3, pwm, channel_dios):
    a, _ = channel_dios[0]
    pwmclk = open_channels(fw, pwm, 1, freq=10000, mode="center", dead="off")
    fw.pwm.duty(pwm["module"], 50)
    for frequency in (10000, 25000):
        fw.pwm.freq(pwm["module"], frequency)
        capture = record(ad3, pwm, frequency, a)
        assert capture.frequency(a) == pytest.approx(expect.pwm_frequency(pwmclk, frequency, "center"), rel=tolerance(pwm, "frequency"))
    fw.pwm.stop(pwm["module"])
    capture = record(ad3, pwm, 25000)
    assert all(capture.edge_count(dio) == 0 for pair in channel_dios[:1] for dio in pair), "outputs toggle after stop"


@pytest.mark.board_params("source", "pwm.count.sources")
@pytest.mark.board_params("frequency", "pwm.count.frequencies_hz")
def test_interrupt_count(fw, pwm, source, frequency):
    count_cfg = pwm["count"]
    channel = pwm["channels"][0]
    open_channels(fw, pwm, 1, freq=frequency, mode="center", dead="off", irq=source)
    fw.pwm.duty(pwm["module"], 50)
    fw.pwm.count(pwm["module"], channel["gen"], clear=True)
    fw.system.delay(count_cfg["window_ms"])
    count = fw.pwm.count(pwm["module"], channel["gen"])
    expected = frequency * count_cfg["window_ms"] / 1000
    assert expected * (1 - count_cfg["tolerance"]) <= count <= expected * (1 + count_cfg["tolerance"]) + frequency * 0.02


@pytest.mark.family("tm4c123")
def test_fault_unsupported(fw, pwm):
    fw.pwm.open(pwm["module"])
    with pytest.raises(FirmwareError) as error:
        fw.pwm.fault(pwm["module"], True)
    assert error.value.reason == "unsupported"


@pytest.mark.family("tm4c129")
def test_fault_tristates_outputs(fw, ad3, pwm, channel_dios, need):
    fault = pwm["fault"]
    wavegen = need.wavegen(fault["drive"])
    ad3.wavegen.dc(wavegen, fault["safe_v"])
    fw.pwm.open(pwm["module"])
    fw.adc.open(0, 0, dcmp=[tuple(entry) for entry in fault["dcmp"]])
    fw.pwm.duty(pwm["module"], *([50] * len(channel_dios)))
    fw.pwm.fault(pwm["module"], True)
    frequency = pwm["efoc"]["frequency_hz"]
    running = record(ad3, pwm, frequency)
    assert all(running.edge_count(a) > 0 for a, _ in channel_dios), "outputs must run before the fault"
    fw.terminal.drain_events("pwm")
    ad3.wavegen.dc(wavegen, fault["trip_v"])
    event = fw.terminal.wait_event("pwm", lambda e: "fault" in e, timeout=fault["timeout_s"])
    assert event.as_int("fault") != 0
    stopped = record(ad3, pwm, frequency)
    for a, b in channel_dios:
        assert stopped.edge_count(a) == 0 and stopped.edge_count(b) == 0, "outputs must stop on a fault"
    ad3.wavegen.dc(wavegen, fault["safe_v"])
    fw.pwm.fault(pwm["module"], False)
