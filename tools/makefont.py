#!/usr/bin/python3

import argparse
import functools
import re

# pattern for '@' after some '.'
BITMAP_PATTERN = re.compile(r'([.@]+)')

def compile(src: str) -> bytes:
	# remove the spaces to the left of the string
	src = src.lstrip()
	result = []

	for line in src.splitlines():
		m = BITMAP_PATTERN.match(line)
		# skip lines not matching bitmap pattern
		if not m:
			continue
		
		# convert .@ pattern to 01 array (e.g. "@@@....." to [1,1,1,0,0,0,0,0]
		bits = [(0 if x == '.' else 1) for x in m.group(1)]
		# convert 01 array to integer (e.g. [1,1,1,0,0,0,0,0] to 224)
		bits_int = functools.reduce(lambda a, b: 2*a + b, bits)
		# integer to binary(224 to "11100000")
		# byteorder does not change the result as the result is appended by each byte
		result.append(bits_int.to_bytes(1, byteorder='little'))
	
	return b''.join(result)

def main():
	# create parser
	parser = argparse.ArgumentParser()
	# configure positional (= mandatory) argument
	parser.add_argument('font', help='path to a font file')
	# configure optional argument (optional arguments will be identified by the - prefix)
	parser.add_argument('-o', help='path to an output file', default='font.out')
	ns = parser.parse_args()

	# open output file in write and binary mode
	# open input file in read mode (read is default when the mode is omitted)
	with open(ns.o, 'wb') as out, open(ns.font) as font:
		src = font.read()
		out.write(compile(src))

if __name__ == '__main__':
	main()