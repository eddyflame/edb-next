## Description
Briefly describe the changes introduced in this PR. Mention if it addresses an existing issue (e.g., `Fixes #123`).

## Type of Change
- [ ] Bug fix (non-breaking change which fixes an issue)
- [ ] New feature (non-breaking change which adds functionality)
- [ ] Breaking change (fix or feature that would cause existing functionality to not work as expected)
- [ ] Documentation update
- [ ] Performance optimization / Refactoring

## Verification & Testing
Describe the tests you ran to verify your changes:
- [ ] `./build/test_core` passed
- [ ] `./build/test_advanced` passed
- [ ] `./build/test_exit` passed
- [ ] Added new unit tests in `tests/` covering this change
- [ ] Manual testing with GUI (`./build/edb_next`)

## Checklist:
- [ ] My code follows the C++20 style guidelines of this project (`clang-format`)
- [ ] I have performed a self-review of my own code
- [ ] I have commented my code, particularly in hard-to-understand areas (e.g., ptrace syscalls)
- [ ] I have updated the documentation accordingly (both English & Chinese where appropriate)
- [ ] My changes generate no new warnings during build
