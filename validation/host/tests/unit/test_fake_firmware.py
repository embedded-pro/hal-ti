import pytest
from ad3_waveforms_bench.terminal import FirmwareError, FirmwareTerminal

from hal_ti_validation.fake_firmware import TM4C129_PINS, FakeFirmware, FakeSerial


class Clock:
    def __init__(self) -> None:
        self.now = 100.0

    def __call__(self) -> float:
        return self.now

    def sleep(self, seconds: float) -> None:
        self.now += seconds


def make_terminal(style="line", chunk=0, noise=False, **kwargs):
    firmware = FakeFirmware(style=style, noise=noise, **kwargs)
    serial = FakeSerial(firmware, chunk=chunk)
    return FirmwareTerminal(serial=serial, timeout=0.5), firmware


def reason(terminal, line):
    response = terminal.command(line, check=False)
    return "ok" if response.ok else response.reason


@pytest.mark.parametrize("style", ["line", "trace"])
@pytest.mark.parametrize("chunk", [0, 3])
@pytest.mark.parametrize("noise", [False, True])
def test_command_framing(style, chunk, noise):
    terminal, firmware = make_terminal(style, chunk, noise)
    boot = terminal.wait_boot(1.0)
    assert boot["board"] == "ek_tm4c123gxl"
    assert boot["reset"] == "por"
    assert terminal.command("ping").ok
    terminal.command("gpio.cfg led0 in")
    firmware.gpio_levels["PF1"] = 1
    assert terminal.command("gpio.get PF1").as_int("value") == 1
    info = terminal.command("info")
    assert info["board"] == "ek_tm4c123gxl"
    assert info["sysclk"] == "80000000"


def test_open_close_busy_and_notopen():
    terminal, _ = make_terminal()
    terminal.command("can.open 0")
    with pytest.raises(FirmwareError) as error:
        terminal.command("can.open 0")
    assert error.value.reason == "busy"
    assert terminal.command("can.close 0").ok
    assert reason(terminal, "can.close 0") == "notopen"
    assert terminal.command("pwm.open 0 gens=0")["pwmclk"] == "80000000"


def test_unrecognized_command_and_unknown_key():
    terminal, _ = make_terminal()
    with pytest.raises(FirmwareError) as error:
        terminal.command("nosuch.command")
    assert error.value.reason == "unrecognized"
    assert reason(terminal, "ping nosuchkey=1") == "usage"


def test_reset_reports_sw_and_closes_instances():
    terminal, firmware = make_terminal("trace")
    terminal.wait_boot(1.0)
    terminal.command("can.open 0")
    terminal.send_nowait("reset")
    boot = terminal.wait_boot(1.0)
    assert boot["reset"] == "sw"
    assert not firmware.opened


def test_sync_clears_partial_input():
    terminal, firmware = make_terminal()
    terminal.write_raw(b"gpio.se")
    terminal.sync()
    assert firmware.received[-1] == "ping"


def test_pins_and_reserved_terminal():
    terminal, _ = make_terminal()
    assert reason(terminal, "gpio.cfg terminaltx in") == "busy"
    assert reason(terminal, "gpio.cfg PA0 in") == "busy"
    assert reason(terminal, "gpio.cfg PG0 in") == "pin", "no port G on TM4C123"
    assert reason(terminal, "gpio.cfg nosuchalias in") == "pin"
    assert reason(terminal, "uart.open 0") == "busy"
    assert reason(terminal, "gpio.cfg m0pwm0 out") == "ok"
    assert reason(terminal, "pwm.open 0 gens=0") == "busy", "PB6 is held by the GPIO group"
    assert reason(terminal, "comp.open 2 neg=PC7") == "range"


@pytest.mark.parametrize(
    ("line", "expected"),
    [
        ("pwm.open 0", "usage"),
        ("pwm.open 0 gens=4", "range"),
        ("pwm.open 0 gens=0,0", "usage"),
        ("pwm.open 0 pins=-:-", "usage"),
        ("pwm.open 0 pins=PB2:-", "pin"),
        ("pwm.open 0 gens=0 pins=PB4:PB5", "pin"),
        ("pwm.open 0 gens=0 freq=1", "range"),
        ("pwm.open 0 gens=0 dead=1000001", "range"),
        ("pwm.open 0 gens=0 dead=60000", "range"),
        ("pwm.open 0 gens=0 div=64 dead=1000000", "ok"),
        ("pwm.open 0 gens=0 dead=1,2,3", "usage"),
        ("pwm.open 0 gens=0,1 trigger=zero,load,none", "usage"),
        ("pwm.open 0 gens=0,1 trigger=zero,load", "ok"),
        ("pwm.open 0 gens=0 irq=zero sync=1", "unsupported"),
        ("pwm.open 0 pins=-:PB7", "ok"),
        ("pwm.open 2 gens=0", "range"),
    ],
)
def test_pwm_open_validation(line, expected):
    terminal, _ = make_terminal()
    assert reason(terminal, line) == expected


def test_pwm_fault_validation():
    terminal, _ = make_terminal()
    assert reason(terminal, "pwm.fault 0 on inputs=1") == "notopen"
    terminal.command("pwm.open 0 gens=0,1")
    assert reason(terminal, "pwm.fault 0 on") == "usage"
    assert reason(terminal, "pwm.fault 0 on inputs=16") == "range"
    assert reason(terminal, "pwm.fault 0 on comparators=256") == "range"
    assert reason(terminal, "pwm.fault 0 on inputs=1 gens=2") == "usage"
    assert reason(terminal, "pwm.fault 0 on inputs=1 pin=PB2") == "pin"
    assert reason(terminal, "pwm.fault 0 on inputs=1 pin=PF4") == "pin", "PF4 is the fault input of module 1"
    assert reason(terminal, "pwm.fault 0 on inputs=1 pin=PD2 gens=1 latch=1 minperiod=65535") == "ok"
    assert reason(terminal, "gpio.cfg PD2 in") == "busy"
    assert reason(terminal, "pwm.fault 0 off latch=1") == "usage"
    assert reason(terminal, "pwm.fault 0 off") == "ok"
    assert reason(terminal, "gpio.cfg PD2 in") == "ok"
    terminal.command("pwm.close 0")
    terminal.command("pwm.open 0 gens=0 sync=1")
    assert reason(terminal, "pwm.fault 0 on inputs=1") == "unsupported"


def test_pwm_fault_keeps_previous_pin_when_new_pin_is_busy():
    terminal, _ = make_terminal()
    terminal.command("pwm.open 0 gens=0")
    assert reason(terminal, "pwm.fault 0 on inputs=1 pin=PD2") == "ok"
    assert reason(terminal, "gpio.cfg PD6 in") == "ok"
    assert reason(terminal, "pwm.fault 0 on inputs=1 pin=PD6") == "busy"
    assert reason(terminal, "gpio.cfg PD2 in") == "busy", "the previous fault pin stays claimed"
    assert reason(terminal, "pwm.fault 0 on inputs=1 pin=PD2") == "ok", "repeating the held pin keeps it"
    assert reason(terminal, "gpio.release PD6") == "ok"
    assert reason(terminal, "pwm.fault 0 on inputs=1 pin=PD6") == "ok"
    assert reason(terminal, "gpio.cfg PD2 in") == "ok", "the replaced fault pin is released"


def test_pwm_interrupt_counts_follow_time():
    clock = Clock()
    terminal, _ = make_terminal(clock=clock, sleep=clock.sleep)
    terminal.command("pwm.open 0 gens=0,1 freq=1000 div=2 mode=edge irq=zero,cmpau")
    assert terminal.command("pwm.count 0 0").as_int("count") == 0, "not running before pwm.duty"
    terminal.command("pwm.duty 0 50")
    terminal.command("delay 200")
    assert terminal.command("pwm.count 0 0 clear=1").as_int("count") == 200
    assert terminal.command("pwm.count 0 1").as_int("count") == 0, "no up-count events in edge mode"
    assert terminal.command("pwm.count 0 0").as_int("count") == 0


def test_adc_validation_and_triggers():
    clock = Clock()
    terminal, firmware = make_terminal(clock=clock, sleep=clock.sleep)
    firmware.adc_codes["PE3"] = 1000
    assert reason(terminal, "adc.open 0 3 sync=1") == "usage"
    assert reason(terminal, "adc.open 0 3 pins=PE3") == "usage", "async needs a trigger"
    assert reason(terminal, "adc.open 0 3 pins=PE3,PE3 sync=1") == "range"
    assert reason(terminal, "adc.open 0 0 pins=PB2 sync=1") == "pin"
    assert reason(terminal, "adc.open 0 1 pins=PE3 delay=4 sync=1") == "unsupported"
    assert reason(terminal, "adc.open 0 1 pins=PE3 ref=ext sync=1") == "unsupported"
    assert reason(terminal, "adc.open 0 1 pins=PE3,PE3 trigger=pwm0 dcmp=0:0:4095,1:0:4095") == "range"
    assert reason(terminal, "adc.open 0 1 pins=PE3,PE3 trigger=pwm0 dcmp=8:0:4095") == "range"
    assert reason(terminal, "adc.open 0 1 pins=PE3,PE3 trigger=pwm0 dcmp=0:9:8") == "range"
    assert reason(terminal, "adc.open 0 1 pins=PE3,PE3 trigger=pwm0 dcmp=0:0:1:top") == "usage"
    assert reason(terminal, "adc.open 0 1 pins=ain0,ain0 trigger=pwm0 dcmp=0:0:1:mid:hyst") == "ok"
    assert reason(terminal, "adc.measure 0 1") == "timeout"
    terminal.command("pwm.open 0 gens=0 freq=100 div=64 trigger=zero")
    assert reason(terminal, "adc.measure 0 1") == "timeout", "the generator does not run yet"
    terminal.command("pwm.duty 0 50")
    start = clock.now
    assert terminal.command("adc.measure 0 1 n=10").as_ints("samples") == [1000] * 10
    assert clock.now - start == pytest.approx(0.1)
    assert reason(terminal, "adc.open 1 3 pins=PE0 sync=1") == "ok"
    assert reason(terminal, "adc.open 1 2 pins=PE0 sync=1") == "busy", "at most two sequencers"


@pytest.mark.parametrize(
    ("filter_option", "ident", "ext", "accepted"),
    [
        ("0x120,0x7f0,0", 0x12F, False, True),
        ("0x120,0x7f0,0", 0x130, False, False),
        ("0x120,0x7f0,0", 0x4800000, True, False),
        ("0x120,0x7f0,0,0", 0x480ABCD, True, True),
        ("0x120,0x7f0,0,0", 0x4C00000, True, False),
        ("0x1abc0000,0x1fff0000,1", 0x1ABCFFFF, True, True),
        ("0x1abc0000,0x1fff0000,1", 0x6AF, False, False),
    ],
)
def test_can_loopback_filter(filter_option, ident, ext, accepted):
    terminal, _ = make_terminal()
    terminal.command(f"can.open 0 loopback=1 filter={filter_option}")
    terminal.command(f"can.send 0 0x{ident:x} 01 ext={int(ext)}")
    frames = terminal.drain_events("can")
    assert bool(frames) == accepted


@pytest.mark.parametrize(
    ("line", "expected"),
    [
        ("can.open 0 bitrate=123457", "range"),
        ("can.open 0 bitrate=500000 timing=12,3,1,10", "usage"),
        ("can.open 0 timing=17,3,1,10", "range"),
        ("can.open 0 timing=12,3,1", "usage"),
        ("can.open 0 filter=0x800,0x7ff,0", "range"),
        ("can.open 0 rx=PF0", "usage"),
        ("can.open 1", "usage"),
        ("can.open 0 timing=12,3,1,10 recover=0", "ok"),
    ],
)
def test_can_open_validation(line, expected):
    terminal, _ = make_terminal()
    assert reason(terminal, line) == expected


@pytest.mark.parametrize(
    ("line", "expected"),
    [
        ("uart.open 1", "ok"),
        ("uart.open 2", "usage"),
        ("uart.open 1 baud=1000", "usage"),
        ("uart.open 1 dma=1 sync=1", "usage"),
        ("uart.open 1 sync=1 parity=even", "unsupported"),
        ("uart.open 1 flow=rts", "usage"),
        ("uart.open 1 flow=cts cts=PC5", "ok"),
        ("spi.open 0 clk=PA2 mosi=PA5", "usage"),
        ("spi.open 0 clk=PA2 mosi=PA5 miso=PA4 cs=PA3 sync=1", "unsupported"),
        ("spi.open 0 clk=PA2 mosi=PA5 miso=PA4 baud=40000001", "range"),
        ("spi.open 0 clk=PA2 mosi=PA5 miso=PA4 baud=40000000", "ok"),
        ("comp.open 0 pos=PC6", "usage"),
        ("comp.open 0 neg=PC7 ref=low,16", "range"),
        ("comp.open 0 neg=PC7 src=pin ref=low,4", "usage"),
        ("comp.open 0 pos=PC6 neg=PC7 trigger=low", "ok"),
        ("qei.open 1", "usage"),
        ("qei.open 0 res=10 offset=10", "range"),
        ("qei.open 0 invi=1", "ok"),
        ("wdt.start 0 timeout=0", "range"),
        ("wdt.start 0 timeout=100 feed=sometimes", "usage"),
        ("wdt.start 0 timeout=100 pin=terminaltx", "busy"),
        ("wdt.feed 0", "notopen"),
    ],
)
def test_open_validation(line, expected):
    terminal, _ = make_terminal()
    assert reason(terminal, line) == expected


def test_comparator_interrupt_rules():
    terminal, _ = make_terminal()
    terminal.command("comp.open 0 pos=PC6 neg=PC7")
    assert reason(terminal, "comp.irq 0 high") == "usage"
    assert reason(terminal, "comp.irq 0 rising") == "ok"
    terminal.command("comp.close 0")
    terminal.command("comp.open 0 pos=PC6 neg=PC7 sync=1")
    assert reason(terminal, "comp.irq 0 rising") == "unsupported"


def test_watchdog_warnings_feeding_and_reset():
    clock = Clock()
    terminal, firmware = make_terminal(clock=clock, sleep=clock.sleep)
    terminal.wait_boot(0.5)
    terminal.command("wdt.start 1 timeout=100 reset=1 feed=manual")
    assert reason(terminal, "wdt.start 0 timeout=100") == "busy"
    clock.now += 0.15
    terminal.command("wdt.feed 1")
    assert [event.as_int("warning") for event in terminal.drain_events("wdt")] == [1]
    clock.now += 0.15
    terminal.pump()
    assert terminal.drain_events("wdt")
    assert not terminal.events("boot")
    clock.now += 0.1
    boot = terminal.wait_boot(0.5)
    assert boot["reset"] == "wdt1"
    assert firmware.watchdog is None


def test_watchdog_auto_feed_never_resets():
    clock = Clock()
    terminal, _ = make_terminal(clock=clock, sleep=clock.sleep)
    terminal.wait_boot(0.5)
    terminal.command("wdt.start 0 timeout=10 feed=auto pin=gpio0")
    assert reason(terminal, "gpio.cfg gpio0 in") == "busy", "the toggle pin stays claimed"
    clock.now += 1.005
    terminal.pump()
    assert len(terminal.drain_events("wdt")) == 100
    assert not terminal.events("boot")


def test_tm4c129_profile():
    terminal, _ = make_terminal(board="ek_tm4c1294xl", family="tm4c129", sysclk=120_000_000, eeprom_size=6144)
    assert reason(terminal, "gpio.cfg PQ3 in") == "ok"
    assert reason(terminal, "gpio.cfg PD4 in") == "busy"
    assert reason(terminal, "uart.open 1") == "usage", "no default UART pins"
    assert reason(terminal, "uart.open 2 tx=PA7 rx=PA6") == "busy", "terminal UART"
    assert reason(terminal, "pwm.open 1 gens=0") == "range"
    assert reason(terminal, "qei.open 1 a=PL1 b=PL2") == "range"
    assert reason(terminal, "pwm.open 0 gens=1") == "ok"
    assert reason(terminal, "eth.open speed=100") == "ok"
    assert terminal.command("eth.status")["link"] == "down"
    assert terminal.command("board.pins").raw.startswith("OK terminaltx=PD5")
    assert TM4C129_PINS["m0pwm6"] == "PK4"
