# Changelog

## [3.0.0](https://github.com/embedded-pro/hal-ti/compare/v2.0.0...v3.0.0) (2026-10-30)


### ⚠ BREAKING CHANGES

* AnalogComparator::Config::routeToPwmFault is removed (it had no effect).
* baud-rate enumerators _38600_bps, _56700_bps and _921000_bps are renamed to _38400_bps, _57600_bps and _921600_bps and now produce the standard rates.
* hal::tiva::WatchDog is renamed to hal::tiva::Watchdog, takes no onExpired, and drops feedTimerInterval and expirationTimeout from Config. Construct a services::EventLoopWatchdog on top of it for the previous behavior.
* **pwm:** Start() takes hal::FractionalPercent; requires embedded-infra-lib 8350cf6b or later.

### Features

* Add async adc ([#9](https://github.com/embedded-pro/hal-ti/issues/9)) ([7893b95](https://github.com/embedded-pro/hal-ti/commit/7893b951eae850a965bc48ca3a48b2dc602c721d))
* Add async pwm ([#72](https://github.com/embedded-pro/hal-ti/issues/72)) ([1dd0ce2](https://github.com/embedded-pro/hal-ti/commit/1dd0ce202990ffdb5d3c3a6adc4eb4c1b8a38b68))
* Add canbus ([#45](https://github.com/embedded-pro/hal-ti/issues/45)) ([27d9309](https://github.com/embedded-pro/hal-ti/commit/27d930929218a58fe0e184769ccc9fc9a2a5fd09))
* Add digital comparator to adc ([#74](https://github.com/embedded-pro/hal-ti/issues/74)) ([6b23e8d](https://github.com/embedded-pro/hal-ti/commit/6b23e8d00d245e19044f61cb544cad06fcd0df92))
* Add dwt ([#8](https://github.com/embedded-pro/hal-ti/issues/8)) ([60cb7f1](https://github.com/embedded-pro/hal-ti/commit/60cb7f1041d42296247721958f7e65270936132d))
* Add eeprom ([#61](https://github.com/embedded-pro/hal-ti/issues/61)) ([72ec2c6](https://github.com/embedded-pro/hal-ti/commit/72ec2c625abdf41fc6b8e24db15fa87459c7ad85))
* Add sync and async analog comparator ([#73](https://github.com/embedded-pro/hal-ti/issues/73)) ([7079808](https://github.com/embedded-pro/hal-ti/commit/707980871c0ecc302c0eedd5f1eadc2a4b7aff1f))
* Add WatchDog driver for TM4C Tiva microcontrollers ([#109](https://github.com/embedded-pro/hal-ti/issues/109)) ([8c451ce](https://github.com/embedded-pro/hal-ti/commit/8c451ce4ca3a871fb5f7205b96a496b96d478e42))
* **demo:** Add demo layer with board demos and an Ethernet demo ([#122](https://github.com/embedded-pro/hal-ti/issues/122)) ([c05b546](https://github.com/embedded-pro/hal-ti/commit/c05b546038f6fcd91bf3f49df77b2ab8908e7372))
* **gpio:** Pool pin interrupt handlers, sized by HAL_TI_GPIO_MAX_INTERRUPT_PINS ([#141](https://github.com/embedded-pro/hal-ti/issues/141)) ([fbb3b79](https://github.com/embedded-pro/hal-ti/commit/fbb3b79a1445641c376bc74839fcef6d2789484d))
* Implement EMIL's watchdog and low-power interfaces for Tiva ([#116](https://github.com/embedded-pro/hal-ti/issues/116)) ([580a896](https://github.com/embedded-pro/hal-ti/commit/580a896a5a5a61f464de8c860ce42559585b9374))
* Merge embedded branch ([#6](https://github.com/embedded-pro/hal-ti/issues/6)) ([9357493](https://github.com/embedded-pro/hal-ti/commit/935749360589249658d04baef659c96c07074219))
* **pwm:** Take duty cycles as a Q16 hal::DutyCycle ([#113](https://github.com/embedded-pro/hal-ti/issues/113)) ([757a1b2](https://github.com/embedded-pro/hal-ti/commit/757a1b27b3681ab51ae2a778e9cb82e0c1c48791))
* **validation:** Add HIL validation app (firmware terminal + Python/AD3 test suite) ([#120](https://github.com/embedded-pro/hal-ti/issues/120)) ([b77237a](https://github.com/embedded-pro/hal-ti/commit/b77237a91c68c709e2a65f292ff98e5c9f693142))


### Bug Fixes

* Adc and pwm fixed ([#29](https://github.com/embedded-pro/hal-ti/issues/29)) ([e5d2339](https://github.com/embedded-pro/hal-ti/commit/e5d23397de885510435b451337561468a143f535))
* **adc:** Drop a batch that is not one whole run instead of asserting ([#140](https://github.com/embedded-pro/hal-ti/issues/140)) ([e9c6dba](https://github.com/embedded-pro/hal-ti/commit/e9c6dba529602063f1c972c4e88531fe8b353b69))
* **adc:** Realign the FIFO after arming and fail on a run of the wrong size ([#137](https://github.com/embedded-pro/hal-ti/issues/137)) ([49a4767](https://github.com/embedded-pro/hal-ti/commit/49a47674685bbec699a291bf07ab7320056f1327))
* **can:** Queue one error notification per class until it has run ([#132](https://github.com/embedded-pro/hal-ti/issues/132)) ([5cf81bc](https://github.com/embedded-pro/hal-ti/commit/5cf81bc47c999d76801ff5208bd803eee5ededef))
* Close the races between the interrupts and the callbacks they invoke ([#130](https://github.com/embedded-pro/hal-ti/issues/130)) ([dead521](https://github.com/embedded-pro/hal-ti/commit/dead521f83cb2cf2e2865d738a9c04692c124b6d))
* Fix qei ([#35](https://github.com/embedded-pro/hal-ti/issues/35)) ([66f773e](https://github.com/embedded-pro/hal-ti/commit/66f773ea7def81befded632d9c9dac22b63855ec))
* Fix synchronous pwm ([#12](https://github.com/embedded-pro/hal-ti/issues/12)) ([dced9c7](https://github.com/embedded-pro/hal-ti/commit/dced9c7a3a7eb57b1aee7856b37ed7e83baa54f0))
* Move SystemCoreClock extern "C" declarations to file scope for GCC 15 compatibility ([#80](https://github.com/embedded-pro/hal-ti/issues/80)) ([1699571](https://github.com/embedded-pro/hal-ti/commit/1699571aeb5a9d69a84299d33112bee28bbf3862))
* Pwm and adc issues ([#38](https://github.com/embedded-pro/hal-ti/issues/38)) ([7015591](https://github.com/embedded-pro/hal-ti/commit/701559186e88e932a6e7abc1c7b14ce297559e29))
* Resolve driver bugs from full-repo assessment ([#117](https://github.com/embedded-pro/hal-ti/issues/117)) ([1b3a182](https://github.com/embedded-pro/hal-ti/commit/1b3a18275bf087de48b380ba33cd564fbf9a94c4))
* Second assessment round — driver bugs, family #ifdef refactor, host tests, docs ([#119](https://github.com/embedded-pro/hal-ti/issues/119)) ([40ed98a](https://github.com/embedded-pro/hal-ti/commit/40ed98a2632b2adec3efb287f3480bdd8449d30e))
* Sync pwm ([#20](https://github.com/embedded-pro/hal-ti/issues/20)) ([1dc90c7](https://github.com/embedded-pro/hal-ti/commit/1dc90c7780cdf02936a18d8b70ee4a2a335d3d2c))
* Uart with dma impl ([#64](https://github.com/embedded-pro/hal-ti/issues/64)) ([86b30ec](https://github.com/embedded-pro/hal-ti/commit/86b30ec8e7ab2c79a54710ed6ef7918fa701b857))
* **watchdog:** Stop the count while a debugger halts the core; feat(gpio): shared AnalogPin ([#133](https://github.com/embedded-pro/hal-ti/issues/133)) ([3ca3eee](https://github.com/embedded-pro/hal-ti/commit/3ca3eee94cf4511ddfa5067afe32b931514ad226))


### Performance Improvements

* **pwm:** Compute dead-time cycles in integer arithmetic ([#136](https://github.com/embedded-pro/hal-ti/issues/136)) ([405b46a](https://github.com/embedded-pro/hal-ti/commit/405b46aab846e9dbc8881ca981ed39de40e61adc))
* **pwm:** Keep the generator masks instead of rebuilding them on every write ([#138](https://github.com/embedded-pro/hal-ti/issues/138)) ([3cd419f](https://github.com/embedded-pro/hal-ti/commit/3cd419f5bf90c279fc89af9b3872a266806b2bbb))
* **pwm:** Write only the comparators while a generator runs ([#135](https://github.com/embedded-pro/hal-ti/issues/135)) ([3cb2025](https://github.com/embedded-pro/hal-ti/commit/3cb2025fc1d6b0b78e87f2b88ffb3c030d5e19a4))
