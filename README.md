# minimu9-ahrs

**minimu9-ahrs** is a program for reading sensor data from the Pololu MinIMU-9
and similar boards over I²C.  It supports the following sensor chips:

* LSM6DS33 accelerometer and gyro
* LSM6DSO accelerometer and gyro
* LIS3MDL magnetometer
* LSM303D magnetometer and accelerometer
* LSM303DLHC magnetometer and accelerometer
* LSM303DLM magnetometer and accelerometer
* LSM303DLH magnetometer and accelerometer
* L3GD20H gyro
* L3GD20 gyro
* L3G4200D gyro

This program works with any board that has a magnetometer, acceleromter, and a
gyro from the list above.  This includes the following Pololu products:

* MinIMU-9 [v0][mv0], [v1][mv1], [v2][mv2], [v3][mv3], [v5][mv5], [v6][mv6]
* AltIMU-10 [v3][av3], [v4][av4], [v5][av5], [v6][av6]
* [Balboa 32U4 Balancing Robot Kit][balboa]

The program can output the raw sensor data from the magnetometor, accelerometer,
and gyro or it can use that raw data to compute the orientation of the IMU.
This program was designed and tested on the Voxl2, but it will probably
work on any embedded Linux board that supports I²C.

## Getting started

### Managing device permissions

After enabling the I²C devices, you should set them up so that your user has
permission to access them.  That way, you won't have to run `sudo`.

First, run `groups` to see what groups your user belongs to.  If `i2c` is on the
list, then that is good.  If you are not on the `i2c` group, then you should add
yourself to it by running `sudo usermod -a -G i2c $(whoami)`, logging out, and
then logging in again.  If your system does not have an `i2c` group, you can
create one or use a different group like `plugdev`.

Next, run `ls -l /dev/i2c*` to make sure that your I²C devices have their group
set to `i2c`.  The group name is shown in the fourth column, and will usually be
`root` or `i2c`.  If the devices are not in the `i2c` group, then you should fix
that by making a file called `/etc/udev.d/rules.d/i2c.rules` with the following
line in it:

    SUBSYSTEM=="i2c-dev" GROUP="i2c"

After making this file, you can make it take effect by running
`sudo udevadm trigger` (or rebooting).

If you get an error about permission being denied, double check that you have done these steps correctly.



### Checking your setup

After you have enabled I²C, given yourself the proper permissions, soldered the
MinIMU-9, and connected it to your board, you should check your setup by
running `i2cdetect`.  Try running `i2cdetect -y N`, where `N` is the number of
the I²C bus you want to use (typically 1 or 0).  The output should look similar
to this:

```
$ i2cdetect -y 1
     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f
00:          -- -- -- -- -- -- -- -- -- -- -- -- --
10: -- -- -- -- -- -- -- -- -- -- -- -- -- 1d -- --
20: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
30: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
40: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
50: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
60: -- -- -- -- -- -- -- -- -- -- -- 6b -- -- -- --
70: -- -- -- -- -- -- -- --
```

The exact output will depend on the type of IMU
you are using, but the important thing is that the body of the printed table
should contain a few hex numbers representing the addresses of I²C devices that
were detected on the bus.

If the `i2cdetect` command is not recognized, you should install the `i2c-tools`
package.  On Ubuntu, you can run `sudo apt-get install i2c-tools` to install
it.

If you do not see a few hex numbers in the body of the table, then make sure
your soldering and wiring are correct and try selecting a different bus by
changing the bus number argument `N`.

If you get a permission denied error, make sure you have configured the device
permissions properly as described above.

If you get a "No such file or directory" error referring to your I²C device,
make sure that you have properly enabled I²C as described above.

### Build

To build `minimu9-ahrs`, you first need to install some libraries that it
depends on.  On Ubuntu, you can install these dependencies by running:

    sudo apt-get install libi2c-dev libeigen3-dev libboost-program-options-dev

Then build using the following commands:

``` 
cd minimu9-ahrs
mkdir build
cd build
cmake ..
make
```

### Looking at raw values

Output format:

magX magY magZ | accX accY accZ | gyrX gyrY gyrZ

Run `minimu9-ahrs --mode raw`.  The output should look something like this:

```
  -1318   -3106   -1801     1896    1219    3679        5      18       3
  -1318   -3106   -1801     1898    1200    3681        0      24      -1
  -1318   -3106   -1801     1899    1200    3688       15      17       2
  -1309   -3105   -1799     1874    1201    3671       17      20      -1
  -1309   -3105   -1799     1898    1214    3663       11      15      -2
```

Output format:

magX magY magZ | accX accY accZ | gyrX gyrY gyrZ

### Calibrating

The magnetometer will need to be calibrated to create a mapping from the
ellipsoid shape of the raw readings to the unit sphere shape that we want the
scaled readings to have.  The calibration feature for the `minimu9-ahrs` assumes
that the shape of the raw readings will be an ellipsoid that is offset from the
origin and stretched along the X, Y, and Z axes.  It cannot handle a rotated
ellipsoid.  It can be informative to run `minimu9-ahrs --mode raw > output.tsv`
while moving the magnetometer and then make some scatter plots of the raw
magnetometer readings in a spreadsheet program to see what shape the readings
have.

To calibrate the magnetometer, run `minimu9-ahrs-calibrate` and follow the
on-screen instructions when they tell you to start rotating the IMU through as
many different orientations as possible.  Once the script has collected enough
data, it saves the data to `~/.minimu9-ahrs-cal-data` and then runs a separate
Python script (`minimu9-ahrs-calibrator`) to create a calibration.

The `minimu9-ahrs-calibrate` script saves the calibration file to
`~/.minimu9-ahrs-cal`.  The calibration file is simply a one-line file with 6
numbers separated by spaces: minimum x, maximum x, minimum y, maximum y, minimum
z, maximum z.  These numbers specify the linear mapping from the raw ellipsoid
to the unit sphere.  For example, if "minimum x" is -414, it means that a
magnetometer reading of -414 on the X axis will get mapped to -1.0 when the
readings are scaled.

### Looking at Euler angles

Run `minimu9-ahrs --output euler`.  It will print a stream of floating-point
numbers, nine per line.  The first three numbers are the yaw, pitch, and roll
angles of the board in degrees.  All three Euler angles should be close zero
when the board is oriented with the Z axis facing down and the X axis facing
towards magnetic north.  From that starting point:

* A positive `yaw` corresponds to a rotation about the Z axis that is
  clockwise when viewed from above.
* A positive `pitch` correspond to a rotation about the Y axis that would
  cause the X axis to aim higher into the sky.
* A positive `roll` would correspond to a counter-clockwise rotation about
  the X axis.

The way you should think about it is that board starts in the neutral position,
then the yaw rotation is applied, then the pitch rotation is applied, and then
the roll rotation is applied to get the board to its final position.

Look at the Euler angle output as you turn the board and make sure that it looks
good.


[ahrs-visualizer]: https://github.com/DavidEGrayson/ahrs-visualizer
[minimu9-ahrs source code]: https://github.com/DavidEGrayson/minimu9-ahrs
[kim]: http://www.nacionale.com/pololu-minimu9-step-by-step/
[av3]: https://www.pololu.com/product/2469
[av4]: https://www.pololu.com/product/2470
[av5]: https://www.pololu.com/product/2739
[av6]: https://www.pololu.com/product/2863
[mv0]: https://www.pololu.com/product/1264
[mv1]: https://www.pololu.com/product/1265
[mv2]: https://www.pololu.com/product/1268
[mv3]: https://www.pololu.com/product/2468
[mv5]: https://www.pololu.com/product/2738
[mv6]: https://www.pololu.com/product/2862
[balboa]: https://www.pololu.com/product/3575
[ffwires]: https://www.pololu.com/catalog/category/66
[wiring_pic]: http://www.davidegrayson.com/minimu9-ahrs/wiring.jpg
[wiring_pic_small]: http://www.davidegrayson.com/minimu9-ahrs/wiring_560px.jpg
[i2cdetect_sample]: https://gist.github.com/DavidEGrayson/2f0531a149d964574565
