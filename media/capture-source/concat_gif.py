#!/usr/bin/env python3
"""Join equal-sized GIFs without quantizing or re-encoding any image data.

Each image receives its source global palette when it has no local palette.
This preserves colors across files, unlike blindly appending GIF packets.
Input loop extensions are replaced with one infinite-loop extension.
"""
import argparse
import struct
from pathlib import Path


def subblocks(data, at):
    while True:
        if at >= len(data):
            raise ValueError('Truncated GIF subblock')
        size = data[at]
        at += size + 1
        if at > len(data):
            raise ValueError('Truncated GIF payload')
        if not size:
            return at


def parse(data):
    if data[:6] not in (b'GIF87a', b'GIF89a') or len(data) < 14:
        raise ValueError('Not a GIF')
    width, height, packed, _, _ = struct.unpack_from('<HHBBB', data, 6)
    size = 3 * (2 << (packed & 7)) if packed & 0x80 else 0
    palette = data[13:13 + size]
    if len(palette) != size:
        raise ValueError('Truncated global palette')
    header, at = data[:13 + size], 13 + size
    blocks, frames = [], 0
    while at < len(data):
        start, marker = at, data[at]
        if marker == 0x3b:
            if at + 1 != len(data):
                raise ValueError('Unexpected trailing data')
            return (width, height), header, blocks, frames
        if marker == 0x21:
            label = data[at + 1]
            at = subblocks(data, at + 2)
            block = data[start:at]
            if label == 0xff and (b'NETSCAPE2.0' in block or b'ANIMEXTS1.0' in block):
                continue
            blocks.append(block)
        elif marker == 0x2c:
            if at + 10 > len(data):
                raise ValueError('Truncated image descriptor')
            descriptor = bytearray(data[at:at + 10])
            local = descriptor[9]
            at += 10
            if local & 0x80:
                at += 3 * (2 << (local & 7))
                image = data[start:at]
            else:
                if not palette:
                    raise ValueError('Image has no palette')
                descriptor[9] = (local & 0x78) | 0x80 | (packed & 7)
                image = bytes(descriptor) + palette
            if at >= len(data):
                raise ValueError('Missing image data')
            end = subblocks(data, at + 1)  # Skip LZW minimum code size.
            blocks.append(image + data[at:end])
            at, frames = end, frames + 1
        else:
            raise ValueError(f'Unexpected GIF block: {marker:#x}')
    raise ValueError('Missing GIF trailer')


def join(inputs):
    parsed = [parse(data) for data in inputs]
    if not parsed or any(item[0] != parsed[0][0] for item in parsed):
        raise ValueError('GIF dimensions must match')
    header = b'GIF89a' + parsed[0][1][6:]
    loop = b'\x21\xff\x0bNETSCAPE2.0\x03\x01\x00\x00\x00'
    result = header + loop + b''.join(block for item in parsed for block in item[2]) + b'\x3b'
    assert parse(result)[3] == sum(item[3] for item in parsed)
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output', type=Path)
    parser.add_argument('inputs', nargs='+', type=Path)
    args = parser.parse_args()
    result = join([path.read_bytes() for path in args.inputs])
    with args.output.open('xb') as output:
        output.write(result)
    print(f'{parse(result)[3]} frames, {len(result)} bytes; original image data preserved')
