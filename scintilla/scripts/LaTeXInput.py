#!/usr/bin/env python3
import sys
import re
import string
import json
from statistics import variance
import time

from FileGenerator import Regenerate

header_path = '../include/LaTeXInput.h'
data_path = '../win32/LaTeXInputData.h'

# strip out unused bytes (sequence[0] and the separator ' ') from input sequences,
# sequence[0] is already stored in `magic` field.
BuildDataForLookupOnly = False

def escape_c_char(ch):
	if ch in r'\'"':
		return '\\' + ch
	if ch < ' ' or ord(ch) >= 127:
		return '\\x%02x' % ord(ch)
	return ch

def quote_c_char(ch):
	return "'" + escape_c_char(ch) + "'"

def make_char_equals(charset):
	lines = []
	for index in range(0, len(charset), 5):
		line = ' || '.join('ch == ' + quote_c_char(ch) for ch in charset[index:index+5])
		lines.append('\t\t|| ' + line)
	return '\n'.join(lines)

def find_word_contains_punctuation(items):
	punctuation = set(string.punctuation)
	punctuation.remove('_')
	result = []
	for item in items:
		if any(ch in punctuation for ch in item):
			result.append(item)
	result.sort()
	return result

def json_dump(obj):
	return json.dumps(obj, ensure_ascii=False, indent='\t')

def json_load(path):
	return json.loads(open(path, encoding='utf-8', newline='\n').read())

def djb2_hash(buf):
	value = 0
	for ch in buf:
		# masked to match C/C++ unsigned integer overflow wrap around
		value = (value*33 + ch) & (2**32 - 1)
	return value


def find_hash_table_size(input_name, input_map, min_hash_size, max_hash_size):
	hash_size = min_hash_size
	hash_distribution = None
	min_collision = sys.maxsize
	key_list = []
	for key, info in input_map.items():
		buf = key.encode('utf-8')
		info['hash_key'] = buf
		key_list.append(buf)

	for size in range(min_hash_size, max_hash_size + 1):
		distribution = [0] * size
		for key in key_list:
			hash_key = djb2_hash(key) % size
			distribution[hash_key] += 1

		max_collision = max(distribution)
		#max_collision = variance(distribution)
		if max_collision < min_collision:
			min_collision = max_collision
			hash_size = size
			hash_distribution = distribution

	min_collision = min(hash_distribution)
	max_collision = max(hash_distribution)
	used = sum(item != 0 for item in hash_distribution)

	print(input_name, 'Hash table size:', (hash_size, used),
		'collision:', (min_collision, max_collision), 'variance:', variance(hash_distribution))
	assert max_collision < 16
	return hash_size

def update_latex_input_data(input_name, input_map, max_hash_size):
	min_hash_size = ord('z') - ord(' ')
	hash_size = max(min_hash_size, round(len(input_map)/16))
	hash_size = find_hash_table_size(input_name, input_map, hash_size, max_hash_size)

	hash_map = {}
	for info in input_map.values():
		hash_key = info['hash_key']
		info['magic'] = len(hash_key) | (hash_key[0] << 8)
		hash_key = djb2_hash(hash_key) % hash_size
		info['hash'] = hash_key
		if hash_key in hash_map:
			hash_map[hash_key].append(info)
		else:
			hash_map[hash_key] = [info]

	hash_table = [0] * hash_size
	input_list = []
	content = []
	offset = 0
	max_collision = 0
	#start_time = time.perf_counter_ns()
	for hash_key in range(hash_size):
		if hash_key in hash_map:
			items = hash_map[hash_key]
			items.sort(key=lambda m: (m['magic'], m['hash_key']))

			prev = 0
			collision = 0
			for item in items:
				magic = item['magic']
				if magic == prev:
					collision += 1
				else:
					if collision > max_collision:
						max_collision = collision
					collision = 0
				prev = magic
			if collision > max_collision:
				max_collision = collision

			if BuildDataForLookupOnly:
				key_list = [info['sequence'][1:] for info in items]
				for index, sequence in enumerate(key_list):
					items[index]['offset'] = offset
					offset += len(sequence)
				content.append('"' + ''.join(key_list) + '"')
			else:
				key_list = [info['sequence'] for info in items]
				for index, sequence in enumerate(key_list):
					items[index]['offset'] = offset + 1
					offset += len(sequence) + 1
				content.append('"' + ' '.join(key_list) + ' "')

			value = (len(input_list) << 4) | len(items)
			assert len(items) < 16, (input_name, hash_key, len(items))
			assert value <= 0xffff, (input_name, hash_key, len(items))
			hash_table[hash_key] = value
			input_list.extend(items)

	#end_time = time.perf_counter_ns()
	#print(input_name, 'loop time:', (end_time - start_time)/1e3)

	output = [f'static const uint16_t {input_name}HashTable[] = {{']
	output.extend('0x%04x,' % value for value in hash_table)
	output.append('};')
	output.append('')

	output.append(f'static const InputSequence {input_name}SequenceList[] = {{')
	prefix = '\\'
	suffix = ''
	if input_name == 'Emoji':
		prefix = '\\:'
		suffix = ':'
	# see https://www.unicode.org/faq/utf_bom.html
	LEAD_OFFSET = 0xD800 - (0x10000 >> 10)
	for info in input_list:
		character = info['character']
		if len(character) == 1:
			ch = ord(character)
			if ch <= 0xffff:
				code = '0x%04X' % ch
			else:
				character = ('U+%X, ' % ch) + character
				# convert to UTF-16
				lead = LEAD_OFFSET + (ch >> 10)
				trail = 0xDC00 + (ch & 0x3FF)
				code = "0x%04X'%04X" % (trail, lead)
		else:
			code = "0x%04X'%04X" % (ord(character[1]), ord(character[0]))
		magic = info['magic']
		offset = info['offset']
		sequence = prefix + info['sequence'] + suffix
		name = info['name']
		line = '{0x%04x, 0x%04x, %s}, // %s, %s, %s' % (magic, offset, code, character, sequence, name)
		output.append(line)
	output.append('};')
	Regenerate(data_path, f'//{input_name} hash', output)

	content[-1] += ';'
	Regenerate(data_path, f'//{input_name} sequences', content)

	max_collision += 1 # maximum string comparison
	size = offset + 2*hash_size + 8*len(input_map)
	print(input_name, 'count:', len(input_map), 'content:', offset,
		'map:', (2*hash_size, 8*len(input_map), max_collision), 'total:', (size, size/1024))

def update_all_latex_input_data(latex_map=None, emoji_map=None):
	if not latex_map:
		latex_map = json_load('latex_map.json')
	if not emoji_map:
		emoji_map = json_load('emoji_map.json')

	update_latex_input_data('LaTeX', latex_map, 512)
	update_latex_input_data('Emoji', emoji_map, 128)


def get_input_map_size_info(input_name, input_map):
	items = [len(key) for key in input_map.keys()]
	min_len = min(items)
	max_len = max(items)
	print(input_name, 'count:', len(items), 'content:', sum(items) + len(items), 'length:', (min_len, max_len))
	return min_len, max_len

def build_charset_function(latex_charset, emoji_charset, output):
	letters = set(string.ascii_letters + string.digits)
	punctuation = set(string.punctuation)
	latex_charset -= letters
	emoji_charset -= letters

	diff = latex_charset - punctuation
	if diff:
		diff = list(sorted(diff))
		print('Invalid LaTeX character:', diff, [ord(ch) for ch in diff])
	diff = emoji_charset - punctuation
	if diff:
		diff = list(sorted(diff))
		print('Invalid Emoji character:', diff, [ord(ch) for ch in diff])

	print('LaTeX punctuation:', latex_charset)
	print('LaTeX punctuation:', [ord(ch) for ch in latex_charset])
	print('Emoji punctuation:', emoji_charset)
	print('Emoji punctuation:', [ord(ch) for ch in emoji_charset])

	latex_punctuation = list(sorted(latex_charset))
	emoji_punctuation = list(sorted(emoji_charset - latex_charset))
	latex_charset = make_char_equals(latex_punctuation)
	emoji_charset = make_char_equals(emoji_punctuation)
	output.extend(f"""
static inline bool IsLaTeXInputSequenceChar(char ch) {{
	return (ch >= 'a' && ch <= 'z')
		|| (ch >= 'A' && ch <= 'Z')
		|| (ch >= '0' && ch <= '9')
{latex_charset}
#if EnableLaTeXLikeEmojiInput
{emoji_charset}
#endif
	;
}}
""".splitlines())

def update_latex_input_header(latex_map, emoji_map, version, link):
	output = ['// input sequences based on ' + version]
	output.append('// documented at ' + link)
	output.append('')

	output.append('enum {')
	min_latex_len, max_latex_len = get_input_map_size_info('LaTeX', latex_map)
	output.append('\tMinLaTeXInputSequenceLength = %d,' % min_latex_len)
	output.append('\tMaxLaTeXInputSequenceLength = %d,' % max_latex_len)
	output.append('')

	min_emoji_len, max_emoji_len = get_input_map_size_info('Emoji', emoji_map)
	prefix = ':'
	suffix = ':'
	output.append('#if EnableLaTeXLikeEmojiInput')
	output.append('\tEmojiInputSequencePrefixLength = %d,' % len(prefix))
	output.append('\tEmojiInputSequenceSuffixLength = %d,' % len(suffix))
	output.append('\tMinEmojiInputSequenceLength = %d + EmojiInputSequencePrefixLength, // suffix is optional' % min_emoji_len)
	output.append('\tMaxEmojiInputSequenceLength = %d + EmojiInputSequencePrefixLength + EmojiInputSequenceSuffixLength,' % max_emoji_len)

	output.append('')
	if max_latex_len >= max_emoji_len + len(prefix) + len(suffix):
		output.append('\tMaxLaTeXInputBufferLength = 1 + MaxLaTeXInputSequenceLength + 1,')
	else:
		output.append('\tMaxLaTeXInputBufferLength = 1 + MaxEmojiInputSequenceLength + 1,')
	output.append('#else')
	output.append('\tMaxLaTeXInputBufferLength = 1 + MaxLaTeXInputSequenceLength + 1,')
	output.append('#endif')
	output.append('};')

	latex_punctuation = find_word_contains_punctuation(latex_map.keys())
	emoji_punctuation = find_word_contains_punctuation(emoji_map.keys())
	print('LaTeX punctuation:', latex_punctuation)
	print('Emoji punctuation:', emoji_punctuation)

	latex_charset = set(''.join(latex_map.keys()))
	emoji_charset = set(''.join(emoji_map.keys()))
	emoji_charset.update(prefix + suffix)
	build_charset_function(latex_charset, emoji_charset, output)
	Regenerate(header_path, '//', output)

def fix_character_and_code(character, code):
	items = re.findall(r'U\+(\w+)', code)
	ch = ''.join(chr(int(item, 16)) for item in items)
	code = ' + '.join('U+' + item for item in items)
	if len(character) != len(ch):
		# add back stripped spaces
		return character in ch, ch, code
	return character == ch, ch, code

def parse_julia_unicode_input_html(path, update_data=True):
	from bs4 import BeautifulSoup

	doc = open(path, encoding='utf-8', newline='\n').read()
	soup = BeautifulSoup(doc, 'html5lib')
	documenter = soup.body.find(id='documenter')
	page = documenter.find(id='documenter-page')

	link = soup.head.find('link', {'rel': 'canonical'})
	link = link['href']
	print(link)

	settings = documenter.find(id='documenter-settings')
	colophon = settings.find('span', {'class': 'colophon-date'})
	version = colophon.parent.get_text().strip()
	version = version[version.index('Julia version'):]
	version_date = colophon.get_text().strip()
	version = f"{version.rstrip('.')} ({version_date}),"
	print(version)

	latex_map = {}
	emoji_map = {}

	table = page.find('table').find('tbody')
	high = sys.maxunicode >> 16
	for row in table.find_all('tr'):
		items = []
		for column in row.find_all('td'):
			items.append(column.get_text().strip())
		if not items: # table header: th
			continue

		row_text = row.get_text().strip()
		assert len(items) == 4, (items, row_text)

		code, character, sequence, name = items
		assert code.startswith('U+'), row_text
		assert sequence.startswith('\\'), row_text

		ok, character, code = fix_character_and_code(character, code)
		assert ok and 1 <= len(character) <= 2, (character, row_text)
		if len(character) == 2:
			assert ord(character[0]) <= 0xffff and high < ord(character[1]) <= 0xffff, (character, row_text)

		items = [item.strip() for item in sequence.split(',')]
		for sequence in items:
			assert len(sequence) > 1 and sequence.startswith('\\'), (sequence, row_text)
			sequence = sequence[1:]

			if sequence[0] == ':':
				assert len(sequence) > 2 and sequence[-1] == ':', (sequence, row_text)
				sequence = sequence[1:-1]
				emoji_map[sequence] = {
					'code': code,
					'character': character,
					'sequence': sequence,
					'name': name
				}
			else:
				latex_map[sequence] = {
					'code': code,
					'character': character,
					'sequence': sequence,
					'name': name
				}

			assert ':' not in sequence, (sequence, row_text)

	with open('latex_map.json', 'w', encoding='utf-8', newline='\n') as fd:
		fd.write(json_dump(latex_map))
	with open('emoji_map.json', 'w', encoding='utf-8', newline='\n') as fd:
		fd.write(json_dump(emoji_map))

	update_latex_input_header(latex_map, emoji_map, version, link)
	if update_data:
		update_all_latex_input_data(latex_map, emoji_map)

# https://docs.julialang.org/en/v1.7-dev/manual/unicode-input/
parse_julia_unicode_input_html('Unicode Input.html')
#update_all_latex_input_data()
