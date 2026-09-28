import pytest
from ad3_waveforms_bench.protocol import parse_event
from ad3_waveforms_bench.terminal import FirmwareError, FirmwareTerminal

from hal_ti_validation.fake_firmware import TM4C123_PINS, FakeFirmware, FakeSerial
from hal_ti_validation.firmware import Dcmp, Firmware, PwmFault


@pytest.fixture
def fake():
    return FakeFirmware()


@pytest.fixture
def fw(fake):
    return Firmware(FirmwareTerminal(serial=FakeSerial(fake, chunk=4), timeout=0.5), TM4C123_PINS)


def last(fake):
    return fake.received[-1]


def test_system(fw, fake):
    fw.system.ping()
    info = fw.system.info()
    assert info.board == "ek_tm4c123gxl"
    assert info.family == "tm4c123"
    assert info.sysclk == 80_000_000
    assert info.uid is None
    assert fw.system.pins() == TM4C123_PINS
    boot = fw.system.reset(timeout=1.0)
    assert boot.reset == "sw"


def test_gpio_resolves_aliases_and_tracks_release(fw, fake):
    fw.gpio.cfg("led0", "out", drive=8)
    fw.gpio.set("led0", 1)
    assert fake.received[-2:] == ["gpio.cfg PF1 out drive=8", "gpio.set PF1 1"]
    assert fw.gpio.get("PF1") == 1
    assert fw.open_instances == [("gpio", "PF1")]
    fw.close_all()
    assert last(fake) == "gpio.release PF1"
    assert fw.open_instances == []


def test_pwm_open_formatting(fw, fake):
    clock = fw.pwm.open(0, gens=[0, 1], pins=[("m0pwm0", None), (None, "m0pwm3")], freq=20000, mode="center", div=2, sync=True)
    assert clock == 40_000_000
    assert last(fake) == "pwm.open 0 gens=0,1 pins=PB6:-,-:PB5 freq=20000 mode=center div=2 sync=1"
    fw.pwm.duty(0, 12.5, 100)
    assert last(fake) == "pwm.duty 0 12.5 100"
    with pytest.raises(FirmwareError) as error:
        fw.pwm.open(0, gens=[2])
    assert error.value.reason == "busy"
    fw.pwm.close(0)
    assert fw.open_instances == []


@pytest.mark.parametrize(
    ("options", "line"),
    [
        ({"dead": 1000}, "dead=1000"),
        ({"dead": (500, 1500)}, "dead=500,1500"),
        ({"dead": "off"}, "dead=off"),
        ({"inva": True, "invb": False}, "inva=1 invb=0"),
        ({"update": "global"}, "update=global"),
        ({"trigger": "cmpad"}, "trigger=cmpad"),
        ({"trigger": ["zero", "none"]}, "trigger=zero,none"),
        ({"irq": ["load", "cmpbu"]}, "irq=load,cmpbu"),
    ],
)
def test_pwm_option_formatting(fw, fake, options, line):
    fw.pwm.open(0, gens=[0, 1], **options)
    assert last(fake) == f"pwm.open 0 gens=0,1 {line}"


def test_pwm_fault_formatting_and_events(fw, fake):
    fw.pwm.open(0, gens=[0, 2])
    fw.pwm.fault(0, gens=[2], comparators=0x81, inputs=1, pin="PD2", latch=True, minperiod=1000)
    assert last(fake) == "pwm.fault 0 on gens=2 comparators=129 inputs=1 pin=PD2 latch=1 minperiod=1000"
    fw.pwm.fault(0, False)
    assert last(fake) == "pwm.fault 0 off"
    fake.event("EVT pwm module=0 gens=4 comparators=1 inputs=0")
    event = fw.pwm.wait_fault(0, timeout=0.5)
    assert event == PwmFault(module=0, gens=4, comparators=1, inputs=0, raw="EVT pwm module=0 gens=4 comparators=1 inputs=0")
    assert PwmFault.from_event(parse_event("EVT pwm module=1 gens=1 comparators=0 inputs=2")).inputs == 2
    fake.event("EVT pwm module=0 gens=1 comparators=0 inputs=1")
    fw.system.ping()
    assert [fault.gens for fault in fw.pwm.faults(0)] == [1]


def test_pwm_count_and_frequency(fw, fake):
    fw.pwm.open(0, gens=[0], irq="zero", div=64, freq=1000)
    fw.pwm.freq(0, 2000)
    assert last(fake) == "pwm.freq 0 2000"
    assert fw.pwm.count(0, 0, clear=True) == 0
    assert last(fake) == "pwm.count 0 0 clear=1"
    fw.pwm.stop(0)
    assert last(fake) == "pwm.stop 0"


def test_close_all_ignores_notopen(fw, fake):
    fw.uart.open(1, baud=115200)
    fake.opened.clear()
    assert fw.close_all() == []


def test_uart_formatting(fw, fake):
    fw.uart.open(1, tx="PC5", rx="PC4", rts="PF0", cts="PF1", baud=921600, parity="even", stop=2, flow="rtscts", dma=True)
    assert last(fake) == "uart.open 1 tx=PC5 rx=PC4 rts=PF0 cts=PF1 baud=921600 parity=even stop=2 flow=rtscts dma=1"
    fw.uart.send(1, b"\x01\xff")
    assert last(fake) == "uart.send 1 01ff"
    fake.uart_rx[1] += b"\xaa"
    assert fw.uart.recv(1, timeout=10, len=1) == b"\xaa"
    assert last(fake) == "uart.recv 1 timeout=10 len=1"


def test_spi_formatting(fw, fake):
    fw.spi.open(0, clk="PA2", mosi="gpio2", miso="gpio1", cs="PA3", baud=4000000, mode=3)
    assert last(fake) == "spi.open 0 clk=PA2 mosi=PA5 miso=PA4 cs=PA3 baud=4000000 mode=3"
    fake.spi_miso = 0xFF
    assert fw.spi.xfer(0, b"", rx=2) == b"\xff\xff"
    assert last(fake) == "spi.xfer 0 - rx=2"
    fw.spi.xfer(0, b"\x12", continue_=True)
    assert last(fake) == "spi.xfer 0 12 continue=1"


def test_adc_formatting(fw, fake):
    fw.adc.open(0, 1, pins=["ain0", "pe2"], sh=16, avg="off", sync=True)
    assert last(fake) == "adc.open 0 1 pins=PE3,PE2 sh=16 avg=off sync=1"
    fw.adc.close(0, 1)
    dcmp = [(0, 100, 3000), Dcmp(1, 0, 4095, "mid"), (2, 5, 6, "low", "hystonce"), Dcmp(3, 1, 2, mode="once")]
    fw.adc.open(0, 0, pins=["ain0"] * 5, delay=7, trigger="pwm1", dcmp=dcmp, ref="ext", prio=2)
    entries = "0:100:3000,1:0:4095:mid,2:5:6:low:hystonce,3:1:2:high:once"
    assert last(fake) == f"adc.open 0 0 pins=PE3,PE3,PE3,PE3,PE3 delay=7 trigger=pwm1 dcmp={entries} ref=ext prio=2"
    fw.adc.open(1, 3, pins=["ain3"], delay="off", sync=True)
    fake.adc_codes["PE0"] = 1234
    assert fw.adc.measure(1, 3, n=3) == [1234] * 3


def test_comparator_formatting(fw, fake):
    fw.comp.open(0, pos="PC6", neg="PC7", src="ref", ref=("low", 8), invert=False, trigger="high")
    assert last(fake) == "comp.open 0 pos=PC6 neg=PC7 src=ref ref=low,8 invert=0 trigger=high"
    with pytest.raises(ValueError):
        fw.comp.open(1, pos="PC5")


def test_qei_formatting(fw, fake):
    fw.qei.open(
        0,
        a="qei0a",
        b="qei0b",
        idx="qei0idx",
        res=100,
        offset=5,
        inva=True,
        invb=False,
        invi=True,
        reset="index",
        cap="a",
        sig="clkdir",
        vel=500,
    )
    assert last(fake) == "qei.open 0 a=PD6 b=PD7 idx=PD3 res=100 offset=5 inva=1 invb=0 invi=1 reset=index cap=a sig=clkdir vel=500"
    reading = fw.qei.read(0)
    assert (reading.pos, reading.dir, reading.res) == (5, "fwd", 100)


def test_can_formatting_and_loopback_frame(fw, fake):
    fw.can.open(0, rx="can0rx", tx="can0tx", bitrate=500000, filter=(0x120, 0x7F0, False), loopback=True, recover=False)
    assert last(fake) == "can.open 0 rx=PF0 tx=PF3 bitrate=500000 filter=0x120,0x7f0,0 loopback=1 recover=0"
    fw.can.send(0, 0x123, b"\x01\x02", ext=False)
    frame = fw.can.wait_frame(0, timeout=0.5)
    assert (frame.id, frame.ext, frame.data) == (0x123, False, b"\x01\x02")
    fw.can.close(0)
    fw.can.open(0, timing=(12, 3, 1, 10), filter=(0x1ABC0000, 0x1FFF0000, True, False), loopback=True)
    assert last(fake) == "can.open 0 timing=12,3,1,10 filter=0x1abc0000,0x1fff0000,1,0 loopback=1"


def test_eeprom_roundtrip(fw):
    fw.eeprom.write(8, b"\xde\xad\xbe\xef")
    assert fw.eeprom.read(8, 4) == b"\xde\xad\xbe\xef"
    fw.eeprom.erase()
    assert fw.eeprom.read(8, 4) == b"\xff" * 4
    with pytest.raises(FirmwareError) as error:
        fw.eeprom.read(2046, 4)
    assert error.value.reason == "range"


def test_watchdog_formatting(fw, fake):
    fw.wdt.start(1, timeout=250, reset=False, feed="manual", pin="gpio0")
    assert last(fake) == "wdt.start 1 timeout=250 reset=0 feed=manual pin=PA2"
    fw.wdt.feed(1)
    assert last(fake) == "wdt.feed 1"


def test_ethernet_unsupported_on_tm4c123(fw):
    with pytest.raises(FirmwareError) as error:
        fw.eth.open(speed=100)
    assert error.value.reason == "unsupported"
