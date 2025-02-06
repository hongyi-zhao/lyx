"""Script used to generate unicode_alphanum_variants."""

import unicodedata
from typing import List


# If True, prints the actual characters to the terminal (which thus requires
# support for Unicode and an appropriate font). Otherwise, prints the value
# that LyX can consume (hexadecimal codepoints).
DEBUG = False


def get_lower_latin_letter_variant_names(character_name: str) -> List[str]:
    return [f'mathematical bold small {character_name}',
            f'mathematical italic small {character_name}',
            f'mathematical bold italic small {character_name}',
            f'mathematical script small {character_name}',
            f'mathematical bold script small {character_name}',
            f'mathematical fraktur small {character_name}',
            f'mathematical bold fraktur small {character_name}',
            f'mathematical double-struck small {character_name}',
            f'mathematical sans-serif small {character_name}',
            f'mathematical sans-serif bold small {character_name}',
            f'mathematical sans-serif italic small {character_name}',
            f'mathematical sans-serif bold italic small {character_name}',
            f'mathematical monospace small {character_name}']


def get_upper_latin_letter_variant_names(character_name: str) -> List[str]:
    return [n.replace('small', 'capital') for n in get_lower_latin_letter_variant_names(character_name)]


def get_lower_greek_letter_variant_names(character_name: str) -> List[str]:
    return [f'mathematical bold small {character_name}',
            f'mathematical italic small {character_name}',
            f'mathematical bold italic small {character_name}',
            '',  # No script.
            '',  # No bold script.
            '',  # No fraktur.
            '',  # No bold fraktur.
            '',  # No double-struck.
            '',  # No sans-serif.
            f'mathematical sans-serif bold small {character_name}',
            '',  # No sans-serif italic.
            f'mathematical sans-serif bold italic small {character_name}',
            ''  # No monospace.
            ]


def get_upper_greek_letter_variant_names(character_name: str) -> List[str]:
    return [n.replace('small', 'capital') for n in get_lower_greek_letter_variant_names(character_name)]


def get_number_variant_names(character_name: str) -> List[str]:
    return [f'mathematical bold digit {character_name}',
            '',  # No italic.
            '',  # No bold italic.
            '',  # No script.
            '',  # No bold script.
            '',  # No fraktur.
            '',  # No bold fraktur.
            f'mathematical double-struck digit {character_name}',
            '',  # No sans-serif.
            f'mathematical sans-serif digit {character_name}',
            f'mathematical sans-serif bold digit {character_name}',
            '',  # No sans-serif italic.
            f'mathematical monospace digit {character_name}']


# This table comes from https://en.wikipedia.org/wiki/Letterlike_Symbols.
EXCEPTIONS_MAPPING = {
    # get_lower_latin_letter_variant_names
    'mathematical script small e': '212F',
    'mathematical script small g': '210A',
    'mathematical italic small h': '210E',
    'mathematical script small o': '2134',
    # get_upper_latin_letter_variant_names
    'mathematical script capital b': '212C',
    'mathematical fraktur capital c': '212D',
    'mathematical double-struck capital c': '2102',
    'mathematical script capital e': '2130',
    'mathematical script capital f': '2131',
    'mathematical script capital h': '210B',
    'mathematical fraktur capital h': '210C',
    'mathematical double-struck capital h': '210D',
    'mathematical script capital i': '2110',
    'mathematical fraktur capital i': '2111',
    'mathematical script capital l': '2112',
    'mathematical script capital m': '2133',
    'mathematical double-struck capital n': '2115',
    'mathematical double-struck capital p': '2119',
    'mathematical double-struck capital q': '211A',
    'mathematical script capital r': '211B',
    'mathematical fraktur capital r': '211C',
    'mathematical double-struck capital r': '211D',
    'mathematical fraktur capital z': '2128',
    'mathematical double-struck capital z': '2124'
    # No other exception.
}


def lookup_with_exceptions(char_name: str) -> str:
    # Maybe the character isn't supposed to exist.
    if not char_name:
        return ''

    # Some characters don't exist in the usual, recent mathematical block, but
    # existed in Unicode 1.x in the Letterlike Symbols block.
    # Usual mapping: drop 'mathematical', replace 'fraktur' by 'black-letter'.
    try:
        return unicodedata.lookup(char_name)
    except KeyError as e:
        char_name = char_name.lower()
        if char_name in EXCEPTIONS_MAPPING:
            return chr(int(EXCEPTIONS_MAPPING[char_name], 16))

        if DEBUG:
            print(char_name)
        else:
            raise e


def format_variants(variants: List[str]) -> str:
    return ' '.join([hex(ord(v)) if v != '' else '""' for v in variants])


print("# Lower-case Latin letters")
for letter_index in range(26):
    letter = chr(ord('a') + letter_index)  # 'a' to 'z'
    variant_names = get_lower_latin_letter_variant_names(letter)
    variants = [lookup_with_exceptions(char_name) for char_name in variant_names]

    if DEBUG:
        print(variants)
    else:
        print(f'{letter} {format_variants(variants)}')

print("# Upper-case Latin letters")
for letter_index in range(26):
    letter = chr(ord('a') + letter_index)  # 'a' to 'z'
    variant_names = get_upper_latin_letter_variant_names(letter)
    variants = [lookup_with_exceptions(char_name) for char_name in variant_names]

    if DEBUG:
        print(variants)
    else:
        print(f'{letter} {format_variants(variants)}')

# The Unicode Greek alphabet has 25 lower-case letters due to two sigmas
# (middle or final).
print("# Lower-case Greek letters")
for letter_index in range(25):
    # 'alpha' to 'omega', the actual character.
    letter = chr(945 + letter_index)
    # 'alpha' to 'omega' (including 'sigma' and 'final sigma').
    letter_name = unicodedata.name(letter).lower().replace('greek small letter', '').strip()
    variant_names = get_lower_greek_letter_variant_names(letter_name)
    variants = [lookup_with_exceptions(char_name) for char_name in variant_names]

    if DEBUG:
        print(variants)
    else:
        print(f'{hex(ord(letter))} {format_variants(variants)}')

print("# Upper-case Greek letters")
for letter_index in range(25):
    # For upper-case Greek letters, we need to filter out 'final sigma'.
    # It's not sufficient to iterate over the upper-case Greek letters
    # (with ALPHA being 913): there is a gap between RHO and SIGMA for, well,
    # FINAL SIGMA (which, for obvious reasons, does not exist).

    # 'alpha' to 'omega', the actual character.
    letter = chr(945 + letter_index)
    # 'alpha' to 'omega' (still a 'final sigma').
    letter_name = unicodedata.name(letter).lower().replace('greek small letter', '').strip()
    # 'alpha' to 'omega' (without 'final sigma').
    if letter_name == 'final sigma':
        continue
    variant_names = get_upper_greek_letter_variant_names(letter_name)
    variants = [lookup_with_exceptions(char_name) for char_name in variant_names]

    if DEBUG:
        print(variants)
    else:
        print(f'{hex(ord(letter))} {format_variants(variants)}')

print("# Numbers")
for number_index in range(10):
    # '0' to '9', the actual character.
    number = chr(ord('0') + number_index)
    # 'zero' to 'nine'.
    number_name = unicodedata.name(number).lower().replace('digit', '').strip()
    variant_names = get_number_variant_names(number_name)
    variants = [lookup_with_exceptions(char_name) for char_name in variant_names]

    if DEBUG:
        print(variants)
    else:
        print(f'{number} {format_variants(variants)}')
