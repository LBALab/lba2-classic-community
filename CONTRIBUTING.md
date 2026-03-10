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
cmake -B build -DSOUND_BACKEND=sdl -DMVIDEO_BACKEND=smacker
cmake --build build
```

### Code Style

The original codebase uses tabs for indentation. There is an ongoing effort to migrate to 4 spaces -- new contributions should use 4 spaces for indentation.

The codebase mixes C and C++ (C++98) along with x86 assembly (UASM). Please keep changes consistent with the style of the file you are modifying.

### Preservation of Original Code

This project values preservation. When modifying original source files:

- Do not remove original French comments -- they are part of the codebase's history
- Do not remove or alter ASCII art banners in source files
- If you need to add clarifying comments, add them alongside the originals rather than replacing them
- When adding or changing documentation about history or culture, attribute to the **original codebase (lba2-classic)** and note when content was preserved during an ASM→C++ port in this fork

## Contributing to the Documentation

You can also help build the official documentation here: https://github.com/2point21/lba-classic-doc
