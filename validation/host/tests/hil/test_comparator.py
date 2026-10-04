"""Analog comparator (`hal::tiva::AnalogComparator` / `SynchronousAnalogComparator`).

Wiring set `bundle2`: W1 on the positive input, W2 on the negative input (a jumper ties the second comparator's
input to the same channel), the output pin on a DIO when the instance has one; this covers the comparators whose
positive input is C0+ (`src=c0`) too.
The output is high while VIN- < VIN+ (inverted with `invert=1`).

Scenarios: features/comparator.feature.
"""

from __future__ import annotations

import time

import pytest
from ad3_waveforms_bench.terminal import FirmwareError
from pytest_bdd import given, parsers, scenario, then, when

from hal_ti_validation import expect


@pytest.fixture
def comp_cfg(board_cfg):
    return board_cfg.param("comparator")


def settle():
    time.sleep(0.005)


def check_output(fw, ad3, need, index, out, pos_level, neg_level, invert):
    expected = int(pos_level > neg_level) ^ invert
    assert fw.comp.read(index) == expected
    if out is not None:
        assert ad3.dio.read(need.dio(out)) == expected, "output pin"


def find_threshold(fw, ad3, index, wavegen, low, high, step):
    """Lowest VIN- at which the output drops (binary search; output is 1 below the threshold)."""
    while high - low > step:
        middle = (low + high) / 2
        ad3.wavegen.dc(wavegen, middle)
        settle()
        if fw.comp.read(index):
            low = middle
        else:
            high = middle
    return (low + high) / 2


def expected_edges(edge, cycles):
    return {"rising": cycles, "falling": cycles, "both": 2 * cycles}[edge]


def table_options(instance, cells):
    """One row of an options table as `comp.open` options: `its pin` is the instance's pin, `ref` is `<range>,<step>`."""
    options = {}
    for key in ("pos", "neg", "src", "ref", "trigger"):
        if not cells[key]:
            continue
        if key in ("pos", "neg"):
            options[key] = instance[key]
        elif key == "ref":
            ref_range, ref_step = cells[key].split(",")
            options[key] = (ref_range, int(ref_step))
        else:
            options[key] = cells[key]
    return options


@pytest.mark.board_params("instance", "comparator.instances")
@pytest.mark.matrix("comparator.output")
@scenario("comparator.feature", "The output follows the comparison of the inputs")
def test_output_follows_inputs(instance, pos_level, invert, sync):
    pass


@pytest.mark.board_params("instance", "comparator.c0_instances")
@pytest.mark.matrix("comparator.output")
@scenario("comparator.feature", "With C0+ as positive input the output follows the comparison against C0+")
def test_positive_input_from_c0(instance, pos_level, invert, sync):
    pass


@pytest.mark.board_params("instance", "comparator.instances")
@pytest.mark.matrix("comparator.ladder")
@pytest.mark.board_params("sync", values=[0, 1])
@scenario("comparator.feature", "The internal reference ladder sets the threshold")
def test_internal_reference(instance, ref_range, step, sync):
    pass


@pytest.mark.board_params("instance", "comparator.instances")
@pytest.mark.board_params("trigger", "comparator.triggers")
@pytest.mark.board_params("sync", values=[0, 1])
@scenario("comparator.feature", "Every ADC trigger sense is accepted and leaves the comparison intact")
def test_trigger_sense_is_accepted(instance, trigger, sync):
    pass


@pytest.mark.board_params("instance", "comparator.instances")
@pytest.mark.matrix("comparator.irq")
@scenario("comparator.feature", "The interrupt counts the output edges and stops counting when turned off")
def test_interrupt_counts(instance, edge, cycles, invert):
    pass


@scenario("comparator.feature", "Only the asynchronous driver has interrupts, and only on edges")
def test_interrupt_restrictions():
    pass


@scenario("comparator.feature", "Invalid option combinations are refused with their reason")
def test_open_errors():
    pass


@given(parsers.parse("the positive level is at least {margin:g} V away from the negative level"))
def levels_apart(comp_cfg, pos_level, margin):
    if abs(pos_level - comp_cfg["neg_level_v"]) < margin:
        pytest.skip("inputs too close to compare reliably")


@given("the positive input is driven to the positive level")
def positive_driven(ad3, need, instance, pos_level):
    ad3.wavegen.dc(need.wavegen(instance["pos"]), pos_level)


@given("C0+ is driven to the positive level")
def c0_driven(ad3, need, instance, pos_level):
    ad3.wavegen.dc(need.wavegen(instance["c0"]), pos_level)


@given("the negative input is driven to the negative level")
def negative_driven(ad3, need, comp_cfg, instance):
    ad3.wavegen.dc(need.wavegen(instance["neg"]), comp_cfg["neg_level_v"])


@given("the inputs settle")
@when("the output settles")
def settled():
    settle()


@given(
    parsers.parse("the expected reference of the range and step lies more than {margin:g} V inside the wavegen range"),
    target_fixture="reference",
)
def reference_in_range(board_cfg, comp_cfg, ref_range, step, margin):
    expected = expect.comparator_reference(comp_cfg["reference"], ref_range, step)
    low_limit, high_limit = board_cfg.ad3.analog_min, board_cfg.ad3.analog_max
    if not low_limit + margin < expected < high_limit - margin:
        pytest.skip(f"reference {expected:.3f} V is outside the wavegen range")
    return expected


@given("the negative input is driven to the bottom of the wavegen range", target_fixture="neg_wavegen")
def negative_at_bottom(ad3, need, board_cfg, instance):
    wavegen = need.wavegen(instance["neg"])
    ad3.wavegen.dc(wavegen, board_cfg.ad3.analog_min)
    return wavegen


@given("the positive input is wired to a wavegen", target_fixture="pos_wavegen")
def positive_wired(need, instance):
    return need.wavegen(instance["pos"])


@given("the positive input is driven halfway between the interrupt low and high levels")
def positive_halfway(ad3, comp_cfg, pos_wavegen):
    ad3.wavegen.dc(pos_wavegen, (comp_cfg["irq_low_v"] + comp_cfg["irq_high_v"]) / 2)


@given(parsers.parse('the instance is open on its inputs and output with source "{source}", the inversion and the asynchronous driver'))
def open_asynchronous(fw, instance, invert, source):
    fw.comp.open(instance["index"], pos=instance["pos"], neg=instance["neg"], out=instance["out"], src=source, invert=invert, sync=False)


@given("the interrupt on the edge is enabled")
def interrupt_enabled(fw, instance, edge):
    fw.comp.irq(instance["index"], edge)


@given("the interrupt count is cleared")
def count_cleared(fw, instance):
    fw.comp.count(instance["index"], clear=True)


@given("the first instance is open on its inputs with the asynchronous driver", target_fixture="first_instance")
def first_open_asynchronous(fw, comp_cfg):
    instance = comp_cfg["instances"][0]
    fw.comp.open(instance["index"], pos=instance["pos"], neg=instance["neg"], sync=False)
    return instance


@when(parsers.parse('the instance opens on its inputs and output with source "{source}", the inversion and the sync'))
def open_on_pins(fw, instance, invert, sync, source):
    fw.comp.open(instance["index"], pos=instance["pos"], neg=instance["neg"], out=instance["out"], src=source, invert=invert, sync=sync)


@when(parsers.parse('the instance opens on its negative input and output with source "{source}", the inversion and the sync'))
def open_from_c0(fw, instance, invert, sync, source):
    fw.comp.open(instance["index"], neg=instance["neg"], out=instance["out"], src=source, invert=invert, sync=sync)


@when(parsers.parse('the instance opens on its negative input with source "{source}", the range and step and the sync'))
def open_on_reference(fw, instance, ref_range, step, sync, source):
    fw.comp.open(instance["index"], neg=instance["neg"], src=source, ref=(ref_range, step), sync=sync)


@when("the instance opens on its inputs and output with the trigger sense and the sync")
def open_with_trigger(fw, instance, trigger, sync):
    fw.comp.open(instance["index"], pos=instance["pos"], neg=instance["neg"], out=instance["out"], trigger=trigger, sync=sync)


@when("the positive input runs the cycles of a square wave between the interrupt low and high levels at the interrupt frequency")
def square_cycles(ad3, comp_cfg, pos_wavegen, cycles):
    low, high, frequency = comp_cfg["irq_low_v"], comp_cfg["irq_high_v"], comp_cfg["irq_frequency_hz"]
    ad3.wavegen.square(pos_wavegen, low, high, frequency, cycles=cycles)
    ad3.wavegen.wait_done(pos_wavegen, timeout=cycles / frequency + 2)


@when(parsers.parse("the firmware waits {ms:d} ms"))
def firmware_waits(fw, ms):
    fw.system.delay(ms)


@when("the interrupt is turned off")
def interrupt_off(fw, instance):
    fw.comp.irq(instance["index"], "off")


@when(parsers.parse("the positive input runs {number:d} cycles of the square wave"))
def more_square_cycles(ad3, comp_cfg, pos_wavegen, number):
    low, high, frequency = comp_cfg["irq_low_v"], comp_cfg["irq_high_v"], comp_cfg["irq_frequency_hz"]
    ad3.wavegen.square(pos_wavegen, low, high, frequency, cycles=number)
    ad3.wavegen.wait_done(pos_wavegen, timeout=number / frequency + 2)


@when("the first instance is closed and opened again on its inputs with the synchronous driver")
def first_reopened_synchronous(fw, first_instance):
    fw.comp.close(first_instance["index"])
    fw.comp.open(first_instance["index"], pos=first_instance["pos"], neg=first_instance["neg"], sync=True)


@then(
    "the comparator reads whether the positive level is above the negative level, inverted with the inversion, "
    "and so does the output pin if the instance has one"
)
def output_follows(fw, ad3, need, comp_cfg, instance, pos_level, invert):
    check_output(fw, ad3, need, instance["index"], instance["out"], pos_level, comp_cfg["neg_level_v"], invert)


@then(
    "the lowest negative input voltage at which the output drops, found by a binary search of the wavegen range down to the search step, "
    "is the expected reference within the reference tolerance"
)
def threshold_at_reference(fw, ad3, board_cfg, comp_cfg, instance, neg_wavegen, reference):
    low_limit, high_limit = board_cfg.ad3.analog_min, board_cfg.ad3.analog_max
    threshold = find_threshold(fw, ad3, instance["index"], neg_wavegen, low_limit, high_limit, comp_cfg["ref_search_step_v"])
    assert threshold == pytest.approx(reference, abs=comp_cfg["ref_tolerance_v"])


@then(
    parsers.parse(
        "with the positive input driven {offset:g} V below and then as far above the negative level, the comparator reads, once the inputs "
        "settle, whether the positive level is above the negative level, not inverted, and so does the output pin if the instance has one"
    )
)
def comparison_intact(fw, ad3, need, comp_cfg, instance, offset):
    neg_level = comp_cfg["neg_level_v"]
    for pos_level in (neg_level - offset, neg_level + offset):
        ad3.wavegen.dc(need.wavegen(instance["pos"]), pos_level)
        settle()
        check_output(fw, ad3, need, instance["index"], instance["out"], pos_level, neg_level, 0)


@then("the interrupt count is the number of cycles, twice that for both edges")
def edges_counted(fw, instance, edge, cycles):
    expected = expected_edges(edge, cycles)
    assert fw.comp.count(instance["index"]) == expected


@then("the interrupt count is still the number of the first cycles, twice that for both edges")
def counting_stopped(fw, instance, edge, cycles):
    expected = expected_edges(edge, cycles)
    assert fw.comp.count(instance["index"]) == expected, "counting must stop after irq off"


@then(parsers.parse('enabling a "{first}" or a "{second}" level interrupt on it fails with "{reason}"'))
def level_interrupts_refused(fw, first_instance, first, second, reason):
    for sense in (first, second):
        with pytest.raises(FirmwareError) as error:
            fw.comp.irq(first_instance["index"], sense)
        assert error.value.reason == reason, sense


@then(parsers.parse('enabling a "{sense}" edge interrupt on it fails with "{reason}"'))
def edge_interrupt_refused(fw, first_instance, sense, reason):
    with pytest.raises(FirmwareError) as error:
        fw.comp.irq(first_instance["index"], sense)
    assert error.value.reason == reason


@then("opening the first instance with each of these options fails with the reason")
def open_refused(fw, comp_cfg, datatable):
    instance = comp_cfg["instances"][0]
    index = instance["index"]
    header, *rows = datatable
    cases = [(table_options(instance, dict(zip(header, row))), row[header.index("reason")]) for row in rows]
    for options, reason in cases:
        with pytest.raises(FirmwareError) as error:
            fw.command("comp.open", index, **{key: fw.pin(value) if key in ("pos", "neg") else value for key, value in options.items()})
        assert error.value.reason == reason, options
