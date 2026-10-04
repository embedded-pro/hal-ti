"""Ethernet (`hal::tiva::Ethernet`, TM4C129 only). The link test needs a cable: `--with ethernet`.

Scenarios: features/ethernet.feature.
"""

import time

import pytest
from ad3_waveforms_bench.terminal import FirmwareError
from pytest_bdd import parsers, scenario, then, when


@scenario("ethernet.feature", "Ethernet is unsupported on the TM4C123")
def test_unsupported_on_tm4c123():
    pass


@scenario("ethernet.feature", "Ethernet opens and reports its status")
def test_open_and_status():
    pass


@pytest.mark.board_params("speed", "ethernet.speeds")
@scenario("ethernet.feature", "The link comes up at the speed")
def test_link_comes_up(speed):
    pass


@when("Ethernet is opened without options")
def open_default(fw):
    fw.eth.open()


@when(parsers.parse("Ethernet is opened on the {phy} PHY at the speed"))
def open_at_speed(fw, speed, phy):
    fw.eth.open(phy=phy, speed=speed)


@then(parsers.parse('opening Ethernet, reading its status and closing it each fail with "{refusal}"'))
def eth_unsupported(fw, refusal):
    for call in (fw.eth.open, fw.eth.status, fw.eth.close):
        with pytest.raises(FirmwareError) as error:
            call()
        assert error.value.reason == refusal


@then("its status reports the link up or down and rx and tx counts of 0 or more")
def status_sane(fw):
    status = fw.eth.status()
    assert status.link in ("up", "down")
    assert status.rx >= 0 and status.tx >= 0


@then(
    parsers.parse("the status reports the link up within the link timeout, read again every {poll_s:g} s while it is not"),
    target_fixture="link_status",
)
def link_up(fw, board_cfg, poll_s):
    deadline = time.monotonic() + board_cfg.param("ethernet.link_timeout_s")
    status = fw.eth.status()
    while status.link != "up" and time.monotonic() < deadline:
        time.sleep(poll_s)
        status = fw.eth.status()
    assert status.link == "up", "no link; is the cable plugged in?"
    return status


@then("the status reports the speed, 10 or 100 Mbit/s when the speed is auto")
def link_speed(link_status, speed):
    if speed != "auto":
        assert link_status.speed == int(speed)
    else:
        assert link_status.speed in (10, 100)


@then("the status reports half or full duplex")
def link_duplex(link_status):
    assert link_status.duplex in ("half", "full")
