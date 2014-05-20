# Lsquaredc (L²C)

Lsquaredc is a tiny Linux library that allows you to access I2C (IIC, I²C, or I squared C) from userspace without causing excessive pain and suffering.

# License

MIT. I believe in freedom, which means I believe in letting you do whatever you want with this code.

# Features

* Small.
* Works.
* Reads and writes.
* Implements repeated start.
* Uses the Bus Pirate convention.

# Limitations

Only master mode is implemented. Only 7-bit addressing is supported. Addressing is fully manual: it is your responsibility to shift the 7-bit I2C address to the left and add the R/W bit (actually, I see this as an advantage).

Obviously, the limitations that the Linux kernel imposes also apply: this library has no way of setting the interface speed, for example.

# Rationale

When I tried accessing I2C on my BeagleBone Black, I was sure it would be obvious. Well, I was wrong. It turned out that the usual way is to read/write data using read() and write() calls, which doesn't support restarts. And if one needs repeated start, there is an ioctl with an entirely different and difficult to use interface.

This library is not something I expected to write, but since I had to, I'm releasing it in the hope that it will save others time and frustration.

# Download

Get it directly from the [Github repository](https://github.com/jwr/lsquaredc).

# Usage

Here's an example of performing a write and then a read with repeated start (restart) from an MMA8453Q accelerometer with a 7-bit address of 0x1c:

```
    uint16_t mma8453_read_interrupt_source[] = {0x38, 0x0c, I2C_RESTART, 0x39, I2C_READ};
    uint8_t status;
    int handle = i2c_open(1);
    i2c_send_sequence(handle, mma8453_read_interrupt_source, 5, &status);
```

First, `i2c_open()` needs to be called. The supplied bus number corresponds to Linux I2C bus numbering: e.g. for "/dev/i2c-1" use bus number 1. Also checks if the device actually supports I2C. Returns the handle which should subsequently be used with   `i2c_send_sequence()`, or a negative number in case of an error.

Data transmission (both transmit and receive) is handled by `i2c_send_sequence()`. It sends a command/data sequence that
can include restarts, writes and reads. Every transmission begins with a START, and ends with a STOP so you do not have
to specify that. 

`i2c_send_sequence()` takes four parameters:

* `handle` is the handle returned from `i2c_open()`
* `sequence` is the I2C operation sequence that should be performed. It can include any number of writes, restarts and reads. Note that the sequence is composed of `uint16_t`, not `uint8_t` elements. This is because we have to support out-of-band signalling of `I2C_RESTART` and `I2C_READ` operations, while still passing through 8-bit data.
* `sequence_length` is the number of sequence elements (not bytes). Sequences of arbitrary (well, 32-bit) length are supported, but there is an upper limit on the number of segments (restarts): no more than 42. This limit is imposed by the Linux ioctl() I2C interface. The minimum sequence length is (rather obviously) 2.
* `received_data` should point to a buffer that can hold as many bytes as there are `I2C_READ` operations in the   sequence. If there are no reads, 0 can be passed, as this parameter will not be used.

`i2c_send_sequence()` uses the Bus Pirate I2C convention, which I found to be very useful and compact. As an example, this
Bus Pirate sequence:

	 "[0x38 0x0c [ 0x39 r ]"

is specified as:

	 {0x38, 0x0c, I2C_RESTART, 0x39, I2C_READ};

in I2C terms, this sequence means:

1. Write 0x0c to device 0x1c (0x0c is usually the register address).
2. Do not release the bus.
3. Issue a repeated start.
4. Read one byte from device 0x1c (which would normally be the contents of register 0x0c on that device).

The sequence may read multiple bytes:

	{0x38, 0x16, I2C_RESTART, 0x39, I2C_READ, I2C_READ, I2C_READ};

This will normally read three bytes from device 0x1c starting at register 0x16. In this case you need to provide a pointer to a buffer than can hold three bytes.

Note that start and stop are added for you automatically, but addressing is fully manual: it is your responsibility to shift the 7-bit I2C address to the left and add the R/W bit. The examples above communicate with a device whose I2C address is 0x1c, which shifted left gives 0x38. For reads we use 0x39, which is `(0x1c<<1)|1`.

If you wonder why I consider the Bus Pirate convention useful, note that what you specify in the sequence is very close to the actual bytes on the wire. This makes debugging and reproducing other sequences easy. Also, you can use the Bus Pirate to prototype, and then easily convert the tested sequences into actual code.

# Devices

I tested this code on a BeagleBone Black and a Raspberry Pi.

## BeagleBone Black

On my BeagleBone Black, pins 19 and 20 on the header correspond to I2C bus 1, but I'm told this can depend on the order in which the kernel chooses to enumerate the busses (oh, the insanity!).

With the default BeagleBone Black, it is enough to compile the example and run it. BeagleBone ships with a working Linux distribution that includes working I2C.

## Raspberry Pi

I tried the official Raspbian (Debian Wheezy) image, which was rather disappointing. I2C is disabled by default (what is wrong with you people?). So I downloaded i2c-tools:

	sudo apt-get install i2c-tools

Then I edited `/etc/modprobe.d/raspi-blacklist.conf`, which blacklists the spi and i2c modules. I removed the line containing `i2c-bcm2708`.

I then added `i2c-dev` to /etc/modules and rebooted.

After these steps, the example worked, but I had to change the bus number to 0 and run the example as root using sudo, because the `/dev/i2c-0` permissions only allow the root user to access the devices (huh?).

Example output:

```
    pi@raspberrypi ~ $ sudo ./example
    Opened bus, result=3
    Sequence processed, result=1
    Sequence processed, result=1
    Sequence processed, result=2
    Status=151
    pi@raspberrypi ~ $
```

## Other devices

Theoretically, this code should also work on any Linux system that has an I2C bus and a driver that supports it and exports a /dev/i2c interface. But, quoting Albert Eistein, “In theory, theory and practice are the same. In practice, they are not.”

# Building and Packaging

You can build the example by simply doing:

	gcc -o lsquaredc-example example.c lsquaredc.c

Packaging? Come on. What packaging? Just put those two files in your project. Or put the git repo in as a subproject. Or package it any way you wish — but I'm afraid I won't be able to help.
