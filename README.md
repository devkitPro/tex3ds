# tex3ds

**3DS Texture Conversion**

```
Usage: ./tex3ds [OPTIONS...] <input>
  Options:
    -f <format>       See "Format Options"
    -m <filter>       Generate mipmaps. See "Mipmap Filter Options"
    -o <output>       Output file
    -p <preview>      Output preview file
    -q <etc1-quality> ETC1 quality. Valid options: low, medium (default), high
    -r, --raw         Output image data only
    -z <compression>  Compress output. See "Compression Options"
    --cubemap         Generate a cubemap. See "Cubemap"
    --skybox          Generate a skybox. See "Skybox"
    <input>           Input file
```

## Format Options

```
    -f rgba, -f rgba8, -f rgba8888
      32-bit RGBA (8-bit components) (default)

    -f rgb, -f rgb8, -f rgb888
      24-bit RGB (8-bit components)

    -f rgba5551
      16-bit RGBA (5-bit RGB, 1-bit Alpha)

    -f rgb565
      16-bit RGB (5-bit Red/Blue, 6-bit Green)

    -f rgba4, -f rgba444
      16-bit RGBA (4-bit components)

    -f la, -f la8, -f la88
      16-bit Luminance/Alpha (8-bit components)

    -f hilo, -f hilo8, -f hilo88
      16-bit HILO (8-bit components)
      Note: HI comes from Red channel, LO comes from Green channel

    -f l, -f l8
      8-bit Luminance

    -f a, -f a8
      8-bit Alpha

    -f la4, -f la44
      8-bit Luminance/Alpha (4-bit components)

    -f l4
      4-bit Luminance

    -f a4
      4-bit Alpha

    -f etc1
      ETC1

    -f etc1a4
      ETC1 with 4-bit Alpha

    -f auto-l8
      L8 when input has no alpha, otherwise LA8

    -f auto-l4
      L4 when input has no alpha, otherwise LA4

    -f auto-etc1
      ETC1 when input has no alpha, otherwise ETC1A4
```

## Mipmap Filter Options

```
    -m bartlett
    -m bessel
    -m blackman
    -m bohman
    -m box
    -m catrom
    -m cosine
    -m cubic
    -m gaussian
    -m hamming
    -m hanning
    -m hermite
    -m jinc
    -m kaiser
    -m lagrange
    -m lanczos
    -m lanczos-radius
    -m lanczos-sharp
    -m lanczos2
    -m lanczos2-sharp
    -m mitchell
    -m parzen
    -m point
    -m quadratic
    -m robidoux
    -m robidoux-sharp
    -m sinc
    -m spline
    -m triangle
    -m welsh
```

## Compression Options

```
    -z auto              Automatically select best compression (default)
    -z none              No compression
    -z huff, -z huffman  Huffman encoding (possible to produce garbage)
    -z lzss, -z lz10     LZSS compression
    -z lz11              LZ11 compression
    -z rle               Run-length encoding

    NOTE: All compression types use a compression header: a single byte which denotes the compression type, followed by four bytes (little-endian) which specify the size of the uncompressed data.

    Types:
      0x00: Fake (uncompressed)
      0x10: LZSS
      0x11: LZ11
      0x28: Huffman encoding
      0x30: Run-length encoding
```

## Cubemap

```
    A cubemap is generated from the input image in the following convention:
    +----+----+---------+
    |    | +Y |         |
    +----+----+----+----+
    | -X | +Z | -X | -Z |
    +----+----+----+----+
    |    | -Y |         |
    +----+----+---------+
```

## Skybox

```
    A skybox is generated from the input image in the following convention:
    +----+----+---------+
    |    | +Y |         |
    +----+----+----+----+
    | -X | -Z | -X | +Z |
    +----+----+----+----+
    |    | -Y |         |
    +----+----+---------+
```
