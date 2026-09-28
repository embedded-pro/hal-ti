"""UART (`hal::tiva::Uart`, `UartWithDma`, `SynchronousUart`) against the AD3 protocol UART.

Wiring set `uart`: firmware TX and RX of each `tests.uart.instances` entry on DIOs; `--with flow` adds RTS/CTS.
"""

import time

import pytest

from hal_ti_validation import expect
from hal_ti_validation.protocol import format_command

pytestmark = pytest.mark.ad3


def open_pair(fw, ad3, need, instance, baud, parity="none", stop=1, variant=None, **extra):
    ad3_rx = need.dio(instance["tx"])
    ad3_tx = need.dio(instance["rx"])
    variant = variant or {}
    fw.uart.open(
        instance["index"],
        tx=instance["tx"],
        rx=instance["rx"],
        baud=baud,
        parity=parity,
        stop=stop,
        dma=variant.get("dma"),
        sync=variant.get("sync"),
        **extra,
    )
    ad3.uart.configure(tx=ad3_tx, rx=ad3_rx, baud=baud, parity=parity, stop=stop)
    ad3.uart.flush()
    fw.uart.recv(instance["index"])


def firmware_to_ad3(fw, ad3, index, payload, baud, parity, stop):
    ad3.uart.flush()
    fw.uart.send(index, payload)
    received = ad3.uart.read(len(payload), timeout=expect.uart_transfer_time(len(payload), baud, parity, stop) + 1.0)
    assert received == payload
    assert ad3.uart.parity_errors == 0


def ad3_to_firmware(fw, ad3, index, payload, baud, parity, stop):
    fw.uart.recv(index)
    ad3.uart.write(payload)
    wait_ms = int(expect.uart_transfer_time(len(payload), baud, parity, stop) * 1000) + 500
    assert fw.uart.recv(index, timeout=wait_ms, len=len(payload)) == payload


@pytest.mark.board_params("instance", "uart.instances")
@pytest.mark.board_params("baud", "uart.bauds")
@pytest.mark.board_params("parity", "uart.parities")
@pytest.mark.board_params("stop", "uart.stop_bits")
@pytest.mark.board_params("variant", "uart.variants")
def test_both_directions(fw, ad3, need, board_cfg, instance, baud, parity, stop, variant):
    open_pair(fw, ad3, need, instance, baud, parity, stop, variant)
    for text in board_cfg.param("uart.payloads"):
        payload = bytes.fromhex(text)
        firmware_to_ad3(fw, ad3, instance["index"], payload, baud, parity, stop)
        ad3_to_firmware(fw, ad3, instance["index"], payload, baud, parity, stop)


@pytest.mark.board_params("instance", "uart.instances")
@pytest.mark.board_params("variant", "uart.variants")
def test_large_payloads(fw, ad3, need, board_cfg, instance, variant):
    baud = 115200
    open_pair(fw, ad3, need, instance, baud, variant=variant)
    prefix = format_command("uart.send", instance["index"])
    size = min(board_cfg.param("uart.large_payload"), expect.max_hex_payload(board_cfg.terminal.max_command_length, prefix))
    firmware_to_ad3(fw, ad3, instance["index"], bytes(range(256))[:size], baud, "none", 1)
    inbound = board_cfg.param("uart.large_payload_to_firmware")
    ad3_to_firmware(fw, ad3, instance["index"], bytes((i * 7) & 0xFF for i in range(inbound)), baud, "none", 1)


@pytest.mark.requires_option("flow")
def test_hardware_flow_control(fw, ad3, need, board_cfg):
    instance = board_cfg.param("uart.flow_instance")
    rts = need.dio(instance["rts"])
    cts = need.dio(instance["cts"])
    baud = 115200
    ad3.dio.drive(cts, 1)
    open_pair(fw, ad3, need, instance, baud, rts=instance["rts"], cts=instance["cts"], flow="rtscts")
    assert ad3.dio.read(rts) == 0, "RTS must be asserted (low) while the firmware can receive"
    payload = bytes.fromhex("0123456789abcdef")
    pending = fw.terminal.begin(format_command("uart.send", instance["index"], payload), timeout=5.0)
    assert ad3.uart.read(len(payload), timeout=0.2) == b"", "data sent while CTS is deasserted"
    ad3.dio.drive(cts, 0)
    assert ad3.uart.read(len(payload), timeout=1.0) == payload
    pending.wait()
    ad3.dio.release(cts)


@pytest.mark.board_params("instance", "uart.instances")
def test_reopen_with_other_settings(fw, ad3, need, instance):
    for baud in (9600, 460800):
        open_pair(fw, ad3, need, instance, baud)
        firmware_to_ad3(fw, ad3, instance["index"], b"\x5a\xa5", baud, "none", 1)
        fw.uart.close(instance["index"])
        time.sleep(0.01)
