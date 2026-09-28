import pytest
from ad3_waveforms_bench.terminal import FirmwareError, FirmwareTerminal

from hal_ti_validation.fake_firmware import FakeFirmware, FakeSerial
from hal_ti_validation.firmware import Firmware

ALIASES = {"ledop": "PF1", "pwm1a": "PB6", "pwm1b": "PB7", "canrx": "PF0", "cantx": "PF3"}


@pytest.fixture
def fake():
    return FakeFirmware(pins=dict(ALIASES))


@pytest.fixture
def fw(fake):
    return Firmware(FirmwareTerminal(serial=FakeSerial(fake, chunk=4), timeout=0.5), ALIASES)


def test_system(fw, fake):
    fw.system.ping()
    info = fw.system.info()
    assert info.board == "ek_tm4c123gxl"
    assert info.family == "tm4c123"
    assert info.sysclk == 80_000_000
    assert info.uid is None
    assert fw.system.pins() == {alias: pin for alias, pin in ALIASES.items()}
    boot = fw.system.reset(timeout=1.0)
    assert boot.reset == "sw"


def test_gpio_resolves_aliases_and_tracks_release(fw, fake):
    fw.gpio.cfg("ledop", "out", drive=8)
    fw.gpio.set("ledop", 1)
    assert fake.received[-2:] == ["gpio.cfg PF1 out drive=8", "gpio.set PF1 1"]
    assert fw.gpio.get("PF1") == 1
    assert fw.open_instances == [("gpio", "PF1")]
    fw.close_all()
    assert fake.received[-1] == "gpio.release PF1"
    assert fw.open_instances == []


def test_pwm_command_formatting(fw, fake):
    clock = fw.pwm.open(0, pins=[("pwm1a", "pwm1b")], gens=[0], freq=20000, mode="center", dead=1000, sync=True)
    assert clock == 40_000_000
    assert fake.received[-1] == "pwm.open 0 gens=0 pins=PB6:PB7 freq=20000 mode=center dead=1000 sync=1"
    fake.handlers["pwm.duty"] = lambda f, a, o: "OK"
    fw.pwm.duty(0, 12.5, 0, 100)
    assert fake.received[-1] == "pwm.duty 0 12.5 0 100"
    with pytest.raises(FirmwareError) as error:
        fw.pwm.open(0)
    assert error.value.reason == "busy"
    fw.pwm.close(0)
    assert fw.open_instances == []


def test_close_all_ignores_notopen(fw, fake):
    fw.uart.open(1, baud=115200)
    fake.opened.clear()
    assert fw.close_all() == []


def test_adc_and_comparator_formatting(fw, fake):
    fw.adc.open(0, 1, pins=["PE3", "pe2"], sh=16, avg="off", dcmp=[(0, 100, 3000)], sync=True)
    assert fake.received[-1] == "adc.open 0 1 pins=PE3,PE2 sh=16 avg=off dcmp=0:100:3000 sync=1"
    fw.comp.open(0, pos="PC6", neg="PC7", src="ref", ref=("low", 8), invert=False)
    assert fake.received[-1] == "comp.open 0 pos=PC6 neg=PC7 src=ref ref=low,8 invert=0"


def test_can_loopback_frame_event(fw, fake):
    fw.can.open(0, rx="canrx", tx="cantx", bitrate=500000, filter=(0x120, 0x7F0, False), loopback=True)
    assert fake.received[-1] == "can.open 0 rx=PF0 tx=PF3 bitrate=500000 filter=0x120,0x7f0,0 loopback=1"
    fw.can.send(0, 0x123, b"\x01\x02", ext=False)
    frame = fw.can.wait_frame(0, timeout=0.5)
    assert (frame.id, frame.ext, frame.data) == (0x123, False, b"\x01\x02")


def test_eeprom_roundtrip(fw):
    fw.eeprom.write(8, b"\xde\xad\xbe\xef")
    assert fw.eeprom.read(8, 4) == b"\xde\xad\xbe\xef"
    fw.eeprom.erase()
    assert fw.eeprom.read(8, 4) == b"\xff" * 4
    with pytest.raises(FirmwareError) as error:
        fw.eeprom.read(2046, 4)
    assert error.value.reason == "range"


def test_spi_xfer_empty_payload_is_dash(fw, fake):
    fake.handlers["spi.xfer"] = lambda f, a, o: "OK rx=0000"
    assert fw.spi.xfer(0, b"", rx=2) == b"\x00\x00"
    assert fake.received[-1] == "spi.xfer 0 - rx=2"
