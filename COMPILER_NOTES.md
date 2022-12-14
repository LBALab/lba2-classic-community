Calling Conventions
===================
Based on text available at source: http://jdebp.info/FGA/function-calling-conventions.html

OS/2 system API calling conventions (Watcom __syscall)
------------------------------------------------------
The OS/2 system API calling convention is, technically, "APIENTRY". That is the macro, defined by the system API C and C++ language header <os2.h>, that expands to whatever the compiler's particular keywords for specifying the requisite calling convention actually are.

"APIENTRY" is, however, two distinct calling conventions, one for 16-bit OS/2 and one for 32-bit OS/2, whose calling conventions differ from each other. The APIENTRY macro expands to two different sets of things, depending from whether one is using the 16-bit OS/2 Developers' Toolkit for OS/2 version 1.x or the 32-bit OS/2 Developers' Toolkit for OS/2 version 2.x.

This is further compounded by the fact that the 32-bit OS/2 version 2.x code can still call the 16-bit OS/2 system API if it wants to. The 32-bit OS/2 Developers' Toolkit's <os2.h> header provides the APIENTRY16 macro, for specifying the 16-bit OS/2 system API calling convention for function declarations in 32-bit code.

### 32-bit OS/2 system API calling convention
> The 32-bit APIENTRY macro corresponds to the following compiler-specific calling convention specifiers:
> * **Watcom C/C++ (32-bit compiler):** __syscall keyword (a.k.a. _syscall and syscall)

### 16-bit OS/2 system API calling convention
> The 32-bit APIENTRY16 macro corresponds to the following compiler-specific calling convention specifiers:
> * **Watcom C/C++ (32-bit compiler):** __far16 __pascal keywords (a.k.a. _far16 _pascal and _Far16 _Pascal)
>
> The 16-bit APIENTRY macro corresponds to the following compiler-specific calling convention specifiers:
> * **Watcom C/C++ (16-bit compiler):** __far __pascal keywords (a.k.a. _far _pascal and far pascal)
> * **Microsoft C/C++ version 6 for OS/2:** far pascal keywords

Compiler-defined calling conventions
------------------------------------
Compiler-defined calling conventions are, as stated, calling conventions defined by a particular compiler. Compilers often define their own calling conventions, in addition to the platform-defined ones. Mainly this is in cases where the compiler-defined calling conventions are in some respects superior to the platform-defined ones, since they can take advantage of the fact that they are usually language-specific, whereas the platform-defined calling conventions have to be language-neutral.

There are two main groupings of compiler-defined calling conventions:
* compiler-defined calling conventions that attempt to provide a common "Microsoft C-like" calling convention 
* compiler-defined calling conventions that take platform-defined stack-based calling conventions and add "go faster" register-based improvements to them

In most cases, a compiler-defined calling convention is in fact the default calling convention used by the compiler, and platform-defined or architecture-defined calling conventions have to be explicitly specified either by compiler command-line options or via explicit calling convention specifiers in the program source code.

Except for the "Microsoft C-like" calling convention, most compiler-defined calling conventions are also compiler specific, in that there's no way to specify exactly the same calling convention with another compiler. Sometimes the keywords look the same, but the conventions differ. Sometimes there simply isn't a keyword at all.

Even Watcom C/C++, whose `#pragma aux` facility allows programmers to define new keywords for user-specified calling conventions, is not flexible enough to be capable of all compiler-defined calling conventions. (A programmer cannot define IBM's Optlink convention with Watcom C/C++'s `#pragma aux`, for example, since it has no mechanism for specifying either placeholders on the call stack or arguments passed in the CPU registers. Watcom C/C++ contains an internal bodge for working around this that is not expressible in source code form.)

"common" __cdecl calling conventions
------------------------------------
> The cdecl calling convention is specified with:
> * **Microsoft C version 6:** cdecl keyword
> * **Microsoft Visual C/C++:** __cdecl keyword
> * **Watcom C/C++:** __cdecl keyword (a.k.a. _cdecl and cdecl)
>
> This is the default calling convention for:
> * Microsoft C/C++ version 5/6
> * Microsoft Visual C/C++

The cdecl calling convention is a last vestige of the days of PC/MS/DR-DOS. Back before the existence of Windows NT, OS/2, or even DOS-Windows, the only (major) operating system API in the world of the IBM PC/AT compatible was the PC/MS/DR-DOS system API. The DOS system API, unlike the system APIs of its aforementioned successors, was not directly callable from high-level languages. Therefore the language bindings to the PC/MS/DR-DOS API comprised wrapper functions, private to the runtime library of each language.

The DOS system API's calling conventions were thus language-defined rather than platform-defined. The DOS API bindings for Microsoft C had one calling convention. The DOS API bindings for Microsoft PASCAL had another. Microsoft FORTRAN and COBOL had specific calling conventions for their DOS API wrappers, too. Thus came about calling conventions known as cdecl, pascal, fortran, and so forth. Each was whatever calling convention the Microsoft compiler for that language used for its DOS API wrapper functions.

The notion of language-defined calling conventions disappeared over twenty years ago. Operating systems with system APIs that were directly callable from high-level languages appeared, and by the time of the DOS-Windows 3.1 Developers' Toolkit, which finally replaced FAR PASCAL with CALLBACK, the notion of the platform-defined calling convention had almost entirely replaced the notion of the language-defined calling convention.

The one remaining vestige of language-defined calling conventions is the common cdecl convention still supported by most C/C++ compilers for x86, both 16-bit and 32-bit. The calling convention remains "whatever Microsoft C used to do" for its 16-bit DOS API wrappers.

Although commonly thought of as the default calling convention for C and C++ compilers, the cdecl calling convention is actually not the default in all but a very few C/C++ compilers. Everyone else defaults to their own compiler-defined calling conventions, defaults to to whatever the target platform's platform-defined system API calling convention is, or (in one oddball case) defaults to the Win32 system API calling convention irrespective of target platform. The reality is that the cdecl calling convention is, everywhere outside of Microsoft's own compilers and 16-bit Borland C/C++, an alternative non-default compatibility option, for doing "whatever Microsoft C used to do" if that is desired.

Most Important Takes on Calling Conventions
-------------------------------------------
### 32-bit
#### Both conventions have in common
* Arguments are pushed onto the call stack by the caller in right-to-left lexical order.
* Arguments of 8-bit and 16-bit integer type are promoted to 32-bit integers. In the cases of variable-argument functions where the type of the parameter is not specified, and of unprototyped functions, arguments of 32-bit floating point type are promoted to 64-bit floating point.
* The direction (DF) flag in the EFLAGS register must be set to zero on entry to and on exit from the function.
* The called function will exit with a simple RET instruction. It is the caller's responsibility to pop the function arguments back off the call stack.
* 8-bit integer, 16-bit integer, 32-bit integer, and 0:32(near) pointer return values will be stored by the called function in the EAX register.
* 64-bit integer and 16:32(far) pointer return values will be stored by the called function in the EAX/EDX register pair.
* Floating point return values will be stored by the called function in the ST(0) FPU register.

#### They differ at
> * __syscall: The following processor registers are volatile: EAX ECX EDX ST(0)–ST(7) GS
> * __cdecl: The following processor registers are volatile: EAX ECX EDX ST(0)–ST(7) GS **ES**
> * __cdecl: With the extended-DOS-targetting DJGPP C/C++ compilers, so too are: FS

> * __syscall: The following processor registers are non-volatile: EBX ESI EDI EBP ESP CS DS **ES FS**
> * __cdecl: The following processor registers are non-volatile: EBX ESI EDI EBP ESP CS DS
> * __cdecl: With the extended-DOS-targetting Watcom C/C++/Fortran compilers, so too are: FS
> * __cdecl: With all OS/2-targetting and Win32-targetting compilers, so too are: FS

> * __syscall: For return values of structure or class type, the caller is expected to allocate space for a value of that type, passing a pointer to it as a hidden parameter, pushed onto the call stack after all other parameters. The called function writes the return value to this address, and returns the address in the EAX register.
> * __cdecl: For return values of structure or class type, there is wide incompatibility amongst compilers. Some make the return thread-safe, by breaking compatibility with the 16-bit cdecl calling convention. Some retain compatibility, at the expense of their 32-bit cdecl calling convention not being thread-safe. The ones that break compatibility don't all agree with one another on how to do so.  

#### Special for __cdecl
> The 32-bit cdecl calling convention is provided by 32-bit compilers that have 16-bit predecesors, and is the logical extension of the 16-bit cdecl calling convention to 32-bits, albeit that there was, of course, no 32-bit Microsoft C for DOS and thus nothing to mimic. It differs from the 16-bit calling convention in several major respects:
> * The EBX register is non-volatile.
> * Whether the FS register is non-volatile depends both from what platform the compiler is targetting and what compiler is being employed. (The Win32 and OS/2 platforms dictate the volatility of FS. But on extended DOS, the choice is left to compilers, and different compilers have different choices.)
> * The return of floating-point values is thread-safe.
> * The return of structure or class type values is incompatible across different compilers.

> Watcom C/C++/Fortran (32-bit compiler) simply does the same as for the 16-bit cdecl calling convention:
> * For return values of structure or class type, the called function is expected to allocate space for a value of that type, writing the return value to this address, and returning the address in the EAX register.
>
> Microsoft Visual C/C++ does the same as what it does for the 32-bit x86 WINAPI calling convention and its own Fastcall calling convention (i.e. It does the same across all three conventions.):
> * For return values of structure or class type that are "plain old data structures" 32 bits or smaller in size, the called function stores the structure/class value in the EAX register.
> * For return values of structure or class type that are "plain old data structures" 33 to 64 bits in size, the called function stores the structure/class value in the EAX/EDX register pair.
> * For return values of structure or class type that are not "plain old data structures" or that are larger than 64 bits, the caller is expected to allocate space for a value of that type, passing a pointer to it as a hidden parameter, pushed onto the call stack after all other parameters. The called function writes the return value to this address, and returns the address in the EAX register.

### 16-bit
#### Both conventions have in common
* Arguments are pushed onto the call stack by the caller in left-to-right lexical order.
* Arguments of 8-bit integer type are promoted to 16-bit integers. In the cases of variable-argument functions where the type of the parameter is not specified, and of unprototyped functions, arguments of 32-bit floating point type are promoted to 64-bit floating point.
* The following processor registers are volatile: AX BX CX DX ES ST(0)–ST(7)
* The following processor registers are non-volatile: SI DI BP SP CS DS FS GS
* The direction (DF) flag in the FLAGS register must be set to zero on entry to and on exit from the function
* 8-bit integer, 16-bit integer, and near pointer return values will be stored by the called function in the AX register.
* The FPU registers are not used to return values.

#### They differ at
> * **__syscall:** The called function will exit with a RETF n instruction, popping the far return address and n additional bytes off the stack as it returns. So whilst non-volatile register SP won't, strictly speaking, have its value exactly preserved across a call, it will be changed by a fixed amount relative to the value that it had upon entry.
> * **__cdecl:** The called function will exit with a simple RET instruction. It is the caller's responsibility to pop the function arguments back off the call stack.

> * **__syscall:** 32-bit integer and 16:16 pointer return values will be stored by the called function in the AX/DX register pair.
> * **__syscall:** 64-bit integers, 0:32 pointers, and 16:32 pointers cannot be returned.
> * **__cdecl:** 64-bit integer, 32-bit integer, and far pointer return values will be stored by the called function in the AX/DX register pair.

> * **__syscall:** For return values of structure or class type and of all floating point types, the caller is expected to allocate space for a value of that type, passing a pointer to it as a hidden parameter, pushed onto the call stack after all other parameters. The called function writes the return value to this address, and returns the address in the EAX register.
> * **__cdecl:** For return values of structure or class type, or of floating-point type, the called function is expected to allocate space for a value of that type, writing the return value to this address, and returning the address in the AX register.
