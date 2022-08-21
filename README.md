# Atari ST Floppy DMA Tester

## What is it?

This tool uses the GEMBIOS `Rwabs()` to read and write data on a floppy and report any error.
My goal was to verify that my Atari 1040 STE floppy DMA was functional.

The test itself is **destructive**, it will overwrite any data present on the tested floppy.
You can select floppy A or B, and it can "test over itself" ie you can copy it on a floppy and test that very same floppy. Then of course you'll have to format and copy it again if you want to use it again.

## Usage

Run `dmatest.tos` and select floppy A or B when prompted.
Alternatively, `dmatest.st` is a 720K disk image prepared for autoboot.
