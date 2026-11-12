/**
 * @file cmdline.h
 * @brief Parse commands in place using spaces to separate arguments and TI-derived code.
 */
//*****************************************************************************
//
// cmdline.h - Prototypes for command line processing functions.
//
// Copyright (c) 2007-2015 Texas Instruments Incorporated.  All rights reserved.
// Software License Agreement
// 
// Texas Instruments (TI) is supplying this software for use solely and
// exclusively on TI's microcontroller products. The software is owned by
// TI and/or its suppliers, and is protected under applicable copyright
// laws. You may not combine this software with "viral" open-source
// software in order to form a larger program.
// 
// THIS SOFTWARE IS PROVIDED "AS IS" AND WITH ALL FAULTS.
// NO WARRANTIES, WHETHER EXPRESS, IMPLIED OR STATUTORY, INCLUDING, BUT
// NOT LIMITED TO, IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE APPLY TO THIS SOFTWARE. TI SHALL NOT, UNDER ANY
// CIRCUMSTANCES, BE LIABLE FOR SPECIAL, INCIDENTAL, OR CONSEQUENTIAL
// DAMAGES, FOR ANY REASON WHATSOEVER.
// 
// This is part of revision 2.1.1.71 of the Tiva Utility Library.
//
//*****************************************************************************

#ifndef __CMDLINE_H__
#define __CMDLINE_H__

//*****************************************************************************
//
// If building with a C++ compiler, make all of the definitions in this header
// have a C binding.
//
//*****************************************************************************
#ifdef __cplusplus
extern "C"
{
#endif

//*****************************************************************************
//
//! \addtogroup cmdline_api
//! @{
//
//*****************************************************************************

//*****************************************************************************
//
//! Defines the value that is returned if the command is not found.
//
//*****************************************************************************
#define CMDLINE_BAD_CMD         (-1)

//*****************************************************************************
//
//! Defines the value that is returned if there are too many arguments.
//
//*****************************************************************************
#define CMDLINE_TOO_MANY_ARGS   (-2)

//*****************************************************************************
//
//! Defines the value that is returned if there are too few arguments.
//
//*****************************************************************************
#define CMDLINE_TOO_FEW_ARGS   (-3)

//*****************************************************************************
//
//! Defines the value that is returned if an argument is invalid.
//
//*****************************************************************************
#define CMDLINE_INVALID_ARG   (-4)

//*****************************************************************************
//
// Command line function callback type.
//
//*****************************************************************************
/**
 * @brief Handle a command synchronously when called by CmdLineProcess().
 * @param argc Supply the number of tokens, including the command name in argv[0].
 * @param argv Supply tokens borrowed from the input buffer. Use argc to determine
 * the number of tokens because the array has no guaranteed NULL terminator.
 * @return The handler returns zero on success or an error code that it defines.
 */
typedef int (*pfnCmdLine)(int argc, char *argv[]);

//*****************************************************************************
//
//! Describe one entry in the command list table.
//
//*****************************************************************************
typedef struct
{
    //
    //! Point to a string containing the command name.
    //
    const char *pcCmd;

    //
    //! Point to the function that implements the command.
    //
    pfnCmdLine pfnCmd;

    //
    //! Point to a string containing brief help text for the command.
    //
    const char *pcHelp;
}
tCmdLineEntry;

//*****************************************************************************
//
//! This is the command table that must be provided by the application.  The
//! last element of the array must be a structure whose pcCmd field contains
//! a NULL pointer.
//
//*****************************************************************************
extern tCmdLineEntry g_psCmdTable[];

//*****************************************************************************
//
// Close the Doxygen group.
//! @}
//
//*****************************************************************************

//*****************************************************************************
//
// Prototypes for the APIs.
//
//*****************************************************************************
/**
 * @brief Tokenize a writable line in place and dispatch an exact command match.
 * @param pcCmdLine Supply a non-NULL, writable string terminated by a NUL byte.
 * The parser replaces spaces with NUL bytes.
 * @return This function returns the handler result when a command matches. It returns
 * CMDLINE_BAD_CMD if the input is blank or the command is unknown. It returns
 * CMDLINE_TOO_MANY_ARGS if the input exceeds the token limit.
 * @note Only literal spaces delimit tokens. No quoting or escaping is supported.
 * The default limit is eight tokens including the command name. Shared argv
 * storage prevents concurrent or nested calls. argv[argc] is not guaranteed
 * to be NULL.
 */
extern int CmdLineProcess(char *pcCmdLine);

//*****************************************************************************
//
// Mark the end of the C bindings section for C++ compilers.
//
//*****************************************************************************
#ifdef __cplusplus
}
#endif

#endif // __CMDLINE_H__
