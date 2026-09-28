"""Ethernet (`hal::tiva::Ethernet`, TM4C129 only). The link test needs a cable: `--with ethernet`."""

import time

import pytest

from hal_ti_validation.terminal import FirmwareError


@pytest.mark.family("tm4c123")
def test_unsupported_on_tm4c123(fw):
    for call in (fw.eth.open, fw.eth.status, fw.eth.close):
        with pytest.raises(FirmwareError) as error:
            call()
        assert error.value.reason == "unsupported"


@pytest.mark.family("tm4c129")
def test_open_and_status(fw):
    fw.eth.open()
    status = fw.eth.status()
    assert status.link in ("up", "down")
    assert status.rx >= 0 and status.tx >= 0


@pytest.mark.family("tm4c129")
@pytest.mark.requires_option("ethernet")
@pytest.mark.board_params("speed", "ethernet.speeds")
def test_link_comes_up(fw, board_cfg, speed):
    fw.eth.open(phy="internal", speed=speed)
    deadline = time.monotonic() + board_cfg.param("ethernet.link_timeout_s")
    status = fw.eth.status()
    while status.link != "up" and time.monotonic() < deadline:
        time.sleep(0.25)
        status = fw.eth.status()
    assert status.link == "up", "no link; is the cable plugged in?"
    if speed != "auto":
        assert status.speed == int(speed)
    else:
        assert status.speed in (10, 100)
    assert status.duplex in ("half", "full")
