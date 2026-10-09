# EK-TM4C1294XL demo

Board bring-up: LEDs, user switches and the trace UART of the EK-TM4C1294XL (TM4C1294NCPDT).

```bash
cmake --preset tm4c1294ncpdt
cmake --build --preset tm4c1294ncpdt-RelWithDebInfo --target demo_ti.ek_tm4c1294xl
```

| Part        | Pins                  | Behaviour                                                            |
|-------------|-----------------------|----------------------------------------------------------------------|
| LED D1      | PN1                   | heartbeat (`services::DebugLed`)                                     |
| LED D2      | PN0                   | SW1 turns it on, SW2 turns it off                                    |
| SW1, SW2    | PJ0, PJ1 (active low) | `services::DebouncedButton`                                          |
| Trace UART0 | PA1 (TX), 115200 8N1  | banner with core clock and unique device ID, then one line per press |

## Ethernet demo

A second firmware in this folder, `demo_ti.ek_tm4c1294xl_ethernet`, brings up the on-board Ethernet with hal-ti's `instantiations::Ethernet` (lwIP over `hal::tiva::Ethernet`, internal PHY, 100 Mbit full duplex) and EMIL's network services. It exists only when `HAL_TI_INCLUDE_LWIP` is on, which the `tm4c1294ncpdt` preset sets.

```bash
cmake --preset tm4c1294ncpdt
cmake --build --preset tm4c1294ncpdt-RelWithDebInfo --target demo_ti.ek_tm4c1294xl_ethernet
```

Plug the board into a network with a DHCP server. Once lwIP has a lease, the trace prints the IPv4 address and the services start:

| Service              | EMIL class                    | Behaviour                                                                   |
|----------------------|-------------------------------|-----------------------------------------------------------------------------|
| DHCP                 | `services::LightweightIp`     | address from the network; the demo is created when the address is available |
| HTTP server, port 80 | `services::DefaultHttpServer` | `/` serves a short page, `/led` toggles LED D2 (PN0) and answers its state  |
| Name resolution      | `services::LlmnrResponder`    | answers LLMNR queries for `ek-tm4c1294xl`, which Windows uses               |

| Part   | Pins | Behaviour                                                         |
|--------|------|-------------------------------------------------------------------|
| LED D1 | PN1  | heartbeat                                                         |
| LED D2 | PN0  | toggled by `GET /led`                                             |
| LED D4 | PF0  | Ethernet link OK, driven by the MAC (`hal::tiva::Ethernet::Leds`) |
| LED D3 | PF4  | Ethernet TX/RX activity, driven by the MAC                        |

1. The MAC address is the one programmed in the flash user registers (the board sticker); if they are blank it falls back to a locally administered address derived from the unique ID.
2. lwIP asks for random numbers (DHCP transaction id, TCP sequence numbers). The TM4C1294NCPDT has no random number generator, so `UniqueIdRandomGenerator` is a xorshift generator seeded from the unique ID. It is not cryptographically secure and is predictable per device.
3. lwIP needs `HAL_GetTick`, which EMIL's lwIP configuration takes from the ST HAL; `hal_tiva/bringup/Bringup.cpp` now provides a weak one from the system tick timer service, as hal-st does.
4. Verified only by building (about 127 KB flash and 36 KB RAM); nothing has been run on hardware, so DHCP, the HTTP pages and LLMNR are untried.
