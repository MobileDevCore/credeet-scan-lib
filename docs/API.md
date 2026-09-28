# API

`POST /api/v1/scan` accepts an image multipart field named `file` and optional `languages` query parameter.

Success contains `items`, `ocr_confidence`, `rotation_degrees`, `image_quality`, and timing fields. Each item contains product ID/name, quantity, unit, confidence, status, confirmation flag and candidates.

Errors use a structured HTTP detail object:
```json
{"success":false,"error_code":"NO_TEXT_DETECTED","message":"No readable text was detected in the image."}
```
Compatibility endpoint: `/scan`.
