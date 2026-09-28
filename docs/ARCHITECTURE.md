# ScanMatch C — Frozen Architecture

```text
Image
  ↓
Image validation / quality
  ↓
Orientation + preprocessing variants
  ↓
Gujarati-first OCR / multilingual fallback
  ↓
Text normalization
  ↓
Line segmentation
  ↓
Product / quantity / unit parsing
  ↓
Catalog matching
  ↓
Confidence engine
  ↓
HIGH → accept
MEDIUM/LOW → confirmation + candidates
  ↓
Structured JSON
  ↓
API / CLI / future application
```

## Gujarati-first design
Gujarati is optimized at the OCR and text-understanding stages, while English and Hindi remain supported. This prevents the common `eng+hin+guj` one-pass strategy from being the only path.

## Matching order
1. Unicode normalization
2. Exact canonical/alias evidence
3. Token overlap
4. Unicode code-point fuzzy similarity
5. Catalog OCR-error variants
6. Brand/catalog evidence
7. Optional embedding callback
8. Confidence and ambiguity check

## Important separation
The parser removes quantity and unit before product matching. Therefore `ચોખા ૫ કિલો` is matched as the product `ચોખા`, with quantity `5` and unit `kg` stored separately.

## Frozen boundary
Future improvements should primarily modify preprocessing, OCR models, Gujarati dictionaries, catalog data, confidence calibration, regression data and performance. The end-to-end stage order should not be repeatedly redesigned.
