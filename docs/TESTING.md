# Testing and Acceptance

Run unit/regression tests with CTest. The parser regression suite covers English, Gujarati and Hindi quantity/unit forms. Add every discovered production bug as a permanent test.

For real-world validation, create a JSONL/CSV ground-truth set containing printed and handwritten English/Gujarati/Hindi/mixed lists across lighting, blur, shadows, rotation, perspective and resolution. Measure character accuracy, word accuracy, SKU accuracy, quantity accuracy, unit accuracy, complete-item accuracy, top-3 recall, false acceptance and latency.

Engineering targets (not current measured results): build/tests 100%, product/quantity/unit ≥95% on the defined set, high-confidence false acceptance <1%, crash rate 0%, English/Hindi/Gujarati ≥95%, mixed ≥90–95%.
