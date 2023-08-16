# Palladium Registry specification

Palladium uses a centralized hierarchical key-value store instead of separate configuration files.  
The registry format described here is used on the main kernel, and for the boot manager configuration database.

This document describes the in-disk format of the registry.  
Any data types without specified endianness should be assumed to have the same endianness as the host machine.

## File Header

Every valid registry file MUST start with the 16-byte header given below.  
The first block of the registry root should directly follow the file header.

|Offset|Length|Field|Value(s)|Description|
|--|--|--|--|--|
|0|4|Signature|REFF|This identifies that the file should contain a valid Registry format file.|
|4|12|Reserved||Reserved for future use.|

## Block Header

A block represents a collection of registry keys/values.  
By default, a block has only a single REMOVED value, encompassing `size of a block - size of the block header`.  
The block size is currently fixed at 2048 bytes (2KiB).

|Offset|Length|Field|Value(s)|Description|
|--|--|--|--|--|
|0|4|Signature|REGB|This identifies that this should be the start of a valid block.|
|4|4|Insert Offset Hint||Offset from the end of the block header into the first free/REMOVED entry. In case the block is full, this field should be set to `0xFFFFFFFF`.|
|8|4|Offset to the Next Block||Offset from the start of the file into the next block associated with this (sub)key.|

## Entry Header

An entry represents a key (which can be thought of as a directory) or a value (which can be thought of as a file).

|Offset|Length|Field|Value(s)|Description|
|--|--|--|--|--|
|0|1|Type|0x00 (REMOVED), 0x01 (BYTE), 0x02 (WORD), 0x03 (DWORD), 0x04 (STRING), 0x05 (BINARY), 0x80 (SUBKEY)|This identifies the type of the entry, and the format of the content coming right after this header.|
|1|2|Length||Size of the entire entry (including this header). This cannot be higher than `size of a block - size of the block header`.|
|3|4|Name Hash||xxHash32 of the name. Used for quickly checking for a match.|
|7||Name||NULL-terminated string to be used when searching for a specific entry. Limited to a max of 32 characters, including the NULL-terminator.|

### Integer Values

On integer values, the entry header is directly followed by 1, 2, 4 or 8 bytes representing a number. The size of the integer (and the amount of bytes) is dependent on the entry type.

### String Values

On string values, the entry header is directly followed by a NULL-terminated string.

### Binary Values

Raw binary data can also be stored (though of a limited size).  
On that case, the entry header is directly followed by a WORD (2 bytes) representing the size of the binary data, and then the binary data, filling the rest of the entry.

### Subkeys

For subkeys, a DWORD (4-bytes) offset from the start of the file into the first block of the subkey is stored after the entry header.
