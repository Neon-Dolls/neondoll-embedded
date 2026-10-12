# Body Identity Contract

## Persistent Identity Format

The body identity is persisted to storage in the following format:
```
v1:{body_id_hex}:{counter}:{timestamp}:{signature_hex}
```

Where:
- `v1` - Version indicator (fixed string)
- `body_id_hex` - 32-byte (256-bit) body ID as lowercase hexadecimal (64 characters)
- `counter` - Monotonically increasing uint32_t as decimal
- `timestamp` - Unix timestamp seconds as decimal
- `signature_hex` - 64-byte (512-bit) Ed25519 signature as lowercase hexadecimal (128 characters)

Example:
```
v1:0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef:0:1730419200:abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef
```

Total length: 4 + 1 + 64 + 1 + up to 10 + 1 + up to 10 + 1 + 128 = variable (typically ~220 characters)

## Field Details

### body_id_hex
- Generated from 16 bytes of cryptographically secure random data
- Encoded as lowercase hexadecimal
- Prefixed with "body_" in runtime representation but stored without prefix in persistent format
- Example: `0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef`

### counter
- Starts at 0 on first boot
- Incremented each time identity is regenerated (though normal operation should preserve identity)
- Stored as decimal string
- Example: `0`, `1`, `42`

### timestamp
- Unix epoch time in seconds when identity was created/last updated
- Stored as decimal string
- Example: `1730419200` (2024-11-01T00:00:00Z)

### signature_hex
- Ed25519 signature of the concatenated fields: `v1:{body_id_hex}:{counter}:{timestamp}`
- Uses a fixed platform-specific key (to be implemented in platform layer)
- Stored as lowercase hexadecimal
- Example: `abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef`

## Validation Rules

When loading identity from storage:
1. Format must match regex `^v1:[0-9a-f]{64}:[0-9]+:[0-9]+:[0-9a-f]{128}$`
2. body_id_hex must be exactly 64 lowercase hex characters
3. counter must be non-negative integer
4. timestamp must be valid Unix timestamp (reasonable range)
5. signature_hex must be exactly 128 lowercase hex characters
6. Signature must verify against the platform key

## Error Conditions

Return -4 (corrupted/incomplete data) when:
- Format doesn't match expected pattern
- Any field has invalid format
- Signature verification fails

Return -5 (version mismatch) when:
- Version indicator is not "v1"

## Implementation Notes

The current implementation uses a JSON-like format for simplicity during development.
Future implementation should migrate to the contract format specified above.

Platform-specific implementations (like ESP-IDF) will provide the actual signing key.
For host testing, a fixed test key may be used.