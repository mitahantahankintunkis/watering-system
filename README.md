# watering-system
I'm too lazy to water my plants, so I automated it by using an Arduino and a small pump. The entire system can automatically water a small area of around 1m^2.

The codebase isn't quite as clean as it could be, since it was made in a short amount of time with a lot of redundancy to make sure that it could be left running reliably for weeks at a time.

## Features
* Soil moisture level measurement
* Automatic soil moisture level upkeep
* Reservoir water level measurement
* Runtime configurement via Bluetooth serial
* Program state managed by a simple state machine
