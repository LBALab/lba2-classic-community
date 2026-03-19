# How to Contribute

If you'd like to contribute to the project, there are several ways you can help.

If you need help getting started, join us on [Discord](https://discord.gg/gfzna5SfZ5).

## Reporting Crashes and Bugs

If you observe a bug you can report it [here](https://github.com/2point21/lba2-classic-community/issues/new).

## Proposing Features

If you have an idea, you can [create a feature request](https://github.com/2point21/lba2-classic-community/issues/new) to suggest a new feature.

## Contributing Code

1. Fork the repository
2. Create a feature branch from `main`
3. Make your changes
4. Submit a pull request

### Setting Up a Development Environment

See the [README](README.md) for prerequisites and build instructions. In short:

```bash
cmake -B build
cmake --build build
```

The default build includes SDL3-based audio and Smacker FMV playback. Override with `-DSOUND_BACKEND=null -DMVIDEO_BACKEND=null` for a minimal build; see the build options table in the README.

### Code Style

The original codebase uses tabs for indentation. There is an ongoing effort to migrate to 4 spaces -- new contributions should use 4 spaces for indentation.

The codebase mixes C and C++ (C++98) along with x86 assembly (UASM). Please keep changes consistent with the style of the file you are modifying.

For C and C++ files, the repository now uses `clang-format` with a checked-in style file. In VS Code, workspace settings enable format-on-save for C and C++ when the recommended `ms-vscode.cpptools` extension is installed.

If you use VS Code, install the workspace recommendations from `.vscode/extensions.json`. The current recommended set is `ms-vscode.cpptools`, `ms-vscode.cmake-tools`, and `EditorConfig.EditorConfig`.

`EditorConfig.EditorConfig` is recommended because this repository also has files that are not auto-formatted by `clang-format`. It makes VS Code honor the checked-in `.editorconfig` for baseline indentation rules, which keeps C/C++ files on 4 spaces and preserves tab-based indentation in ASM and related files.

ASM files are intentionally excluded from automatic formatting, and `LIB386/libsmacker/` is kept out of formatting because it is third-party code.

A small set of preservation-sensitive or macro-heavy files is also excluded from automatic formatting where `clang-format` does not produce stable results yet. Keep those files manual until they are split up or annotated for safer tooling.

If you are doing the one-time whitespace migration or reviewing it locally, keep it as a dedicated formatting-only commit and then configure git blame to ignore that commit:

```bash
git config blame.ignoreRevsFile .git-blame-ignore-revs
```

After the bootstrap formatting commit exists, add its SHA to `.git-blame-ignore-revs`. Do not mix semantic changes into that commit.

### Preservation of Original Code

This project values preservation. When modifying original source files:

- Do not remove original French comments -- they are part of the codebase's history
- Do not remove or alter ASCII art banners in source files
- If you need to add clarifying comments, add them alongside the originals rather than replacing them
- When adding or changing documentation about history or culture, attribute to the **original codebase (lba2-classic)** and note when content was preserved during an ASM→C++ port in this fork

## Contributing to the Documentation

You can also help build the official documentation here: https://github.com/2point21/lba-classic-doc
