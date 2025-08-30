# SPDX-FileCopyrightText: (C) 2025 ilmmatias
# SPDX-License-Identifier: GPL-3.0-or-later

import curses
import re

# Message destinations.
KD_DEST_KERNEL = 0
KD_DEST_COMMAND = 1

# Message types.
KD_TYPE_NONE = 0
KD_TYPE_ERROR = 1
KD_TYPE_TRACE = 2
KD_TYPE_DEBUG = 3
KD_TYPE_INFO = 4

# ANSI color codes.
KDP_ANSI_FG_RED = "\033[38;5;196m"
KDP_ANSI_FG_GREEN = "\033[38;5;46m"
KDP_ANSI_FG_YELLOW = "\033[38;5;226m"
KDP_ANSI_FG_BLUE = "\033[38;5;51m"

KDP_ANSI_RESET = "\033[0m"

# (n)curses color codes.
KDP_COLOR_RED = 196
KDP_COLOR_GREEN = 46
KDP_COLOR_YELLOW = 226
KDP_COLOR_BLUE = 51
KDP_COLOR_GREY = 250

# Layout constants.
KDP_VERTICAL_SPLIT = 0
KDP_HORIZONTAL_SPLIT = 1
KDP_VERTICAL_SPLIT_THRESHOLD = 172

# Internal context.
KdpScreen: curses.window = None
KdpScreenWidth: int = 0
KdpScreenHeight: int = 0

KdpCurrentLayout: int = 0
KdpSelectedOutput: int = KD_DEST_KERNEL

KdpKernelOutputBuffer: list[tuple[str, int]] = []
KdpKernelOutputContainer: curses.window = None
KdpKernelOutputWindow: curses.window = None
KdpKernelOutputWidth: int = 0
KdpKernelOutputHeight: int = 0
KdpKernelOutputMaxLines: int = 16384
KdpKernelOutputCurrentPosition: int = 0
KdpKernelOutputRefreshCoords: tuple[int, int, int, int] = ()

KdpCommandOutputBuffer: list[tuple[str, int]] = []
KdpCommandOutputContainer: curses.window = None
KdpCommandOutputWindow: curses.window = None
KdpCommandOutputWidth: int = 0
KdpCommandOutputHeight: int = 0
KdpCommandOutputMaxLines: int = 16384
KdpCommandOutputCurrentPosition: int = 0
KdpCommandOutputRefreshCoords: tuple[int, int, int, int] = ()

KdpInputWindow: curses.window = None
KdpInputWidth: int = 0
KdpInputHeight: int = 0

#--------------------------------------------------------------------------------------------------
# PURPOSE:
#     This function refreshes the debugger interface after a screen resize.
#
# PARAMETERS:
#     None.
#
# RETURN VALUE:
#     None.
#--------------------------------------------------------------------------------------------------
def KdpRefreshInterface() -> None:
    # Regrab the screen dimensions (they did just change).
    global KdpScreen, KdpScreenWidth, KdpScreenHeight
    KdpScreenHeight, KdpScreenWidth = KdpScreen.getmaxyx()
    KdpScreen.clear()

    # Use a vertical split only if we can safely fit the `rp`/`rv` command output (without
    # line breaks) when doing so.
    global KdpCurrentLayout
    if KdpScreenWidth > KDP_VERTICAL_SPLIT_THRESHOLD:
        KdpCurrentLayout = KDP_VERTICAL_SPLIT
    else:
        KdpCurrentLayout = KDP_HORIZONTAL_SPLIT

    SelectedAttribute = curses.A_BOLD | curses.color_pair(KDP_COLOR_BLUE)
    UnselectedAttribute = curses.A_BOLD

    # Reinitialize most of the available area as a scrollable output for the kernel output.
    global KdpKernelOutputContainer, KdpKernelOutputWindow, KdpKernelOutputRefreshCoords
    global KdpKernelOutputWidth, KdpKernelOutputHeight, KdpKernelOutputMaxLines
    if KdpCurrentLayout == KDP_VERTICAL_SPLIT:
        KdpKernelOutputWidth = KdpScreenWidth // 2
        KdpKernelOutputHeight = KdpScreenHeight - 1
    else:
        KdpKernelOutputWidth = KdpScreenWidth
        KdpKernelOutputHeight = (KdpScreenHeight - 1) // 2

    KdpKernelOutputContainer = curses.newwin(KdpKernelOutputHeight, KdpKernelOutputWidth, 0, 0)
    KdpKernelOutputWindow = curses.newpad(KdpKernelOutputMaxLines, KdpKernelOutputWidth - 2)
    KdpKernelOutputRefreshCoords = (1, 1, KdpKernelOutputHeight - 2, KdpKernelOutputWidth - 2)
    KdpKernelOutputContainer.box()
    KdpKernelOutputContainer.addstr(
        0,
        2,
        " Kernel Output ",
        SelectedAttribute if KdpSelectedOutput == KD_DEST_KERNEL else UnselectedAttribute)
    KdpKernelOutputContainer.refresh()

    # As we reintialized the pad as well, we need to reappend all the current text
    # (and then scroll to the correct position).
    for (Line, Attribute) in KdpKernelOutputBuffer:
        KdpKernelOutputWindow.addstr(Line, Attribute)
    KdpScrollWindow(
        KdpKernelOutputWindow,
        KdpKernelOutputHeight,
        KdpKernelOutputRefreshCoords,
        KdpKernelOutputCurrentPosition,
        0)

    # And most of the remaining area as a scrollable output for the command line output.
    global KdpCommandOutputContainer, KdpCommandOutputWindow, KdpCommandOutputRefreshCoords
    global KdpCommandOutputWidth, KdpCommandOutputHeight, KdpCommandOutputMaxLines
    if KdpCurrentLayout == KDP_VERTICAL_SPLIT:
        KdpCommandOutputWidth = KdpScreenWidth - KdpKernelOutputWidth
        KdpCommandOutputHeight = KdpScreenHeight - 1
        KdpCommandOutputContainer = curses.newwin(
            KdpCommandOutputHeight,
            KdpCommandOutputWidth,
            0,
            KdpKernelOutputWidth)
        KdpCommandOutputRefreshCoords = (
            1,
            KdpKernelOutputWidth + 1,
            KdpCommandOutputHeight - 2,
            KdpScreenWidth - 2)
    else:
        KdpCommandOutputWidth = KdpScreenWidth
        KdpCommandOutputHeight = (KdpScreenHeight - 1) - KdpKernelOutputHeight
        KdpCommandOutputContainer = curses.newwin(
            KdpCommandOutputHeight,
            KdpCommandOutputWidth,
            KdpKernelOutputHeight,
            0)
        KdpCommandOutputRefreshCoords = (
            KdpKernelOutputHeight + 1,
            1,
            KdpScreenHeight - 3,
            KdpScreenWidth - 2)

    KdpCommandOutputWindow = curses.newpad(KdpCommandOutputMaxLines, KdpCommandOutputWidth - 2)
    KdpCommandOutputContainer.box()
    KdpCommandOutputContainer.addstr(
        0,
        2,
        " Command Output ",
        SelectedAttribute if KdpSelectedOutput == KD_DEST_COMMAND else UnselectedAttribute)
    KdpCommandOutputContainer.refresh()

    for (Line, Attribute) in KdpCommandOutputBuffer:
        KdpCommandOutputWindow.addstr(Line, Attribute)
    KdpScrollWindow(
        KdpCommandOutputWindow,
        KdpCommandOutputHeight,
        KdpCommandOutputRefreshCoords,
        KdpCommandOutputCurrentPosition,
        0)

    # And reinitialize the last line as the input area.
    global KdpInputWindow, KdpInputWidth, KdpInputHeight
    KdpInputWidth = KdpScreenWidth
    KdpInputHeight = 1
    KdpInputWindow = curses.newwin(KdpInputHeight, KdpInputWidth, KdpScreenHeight - 1, 0)
    KdpInputWindow.keypad(True)
    KdpInputWindow.nodelay(True)

#--------------------------------------------------------------------------------------------------
# PURPOSE:
#     This function initializes the screen for the debugger interface.
#
# PARAMETERS:
#     None.
#
# RETURN VALUE:
#     None.
#--------------------------------------------------------------------------------------------------
def KdpInitializeInterface() -> None:
    # Initialize the main screen (containing all of the area).
    global KdpScreen
    KdpScreen = curses.initscr()

    # Setup the default (n)curses parameters.
    curses.cbreak()
    curses.curs_set(0)
    curses.nl()
    curses.noecho()

    # Setup the color map for KdpPrint.
    curses.start_color()
    curses.use_default_colors()
    curses.init_pair(KDP_COLOR_RED, KDP_COLOR_RED, -1)
    curses.init_pair(KDP_COLOR_GREEN, KDP_COLOR_GREEN, -1)
    curses.init_pair(KDP_COLOR_YELLOW, KDP_COLOR_YELLOW, -1)
    curses.init_pair(KDP_COLOR_BLUE, KDP_COLOR_BLUE, -1)
    curses.init_pair(KDP_COLOR_GREY, KDP_COLOR_GREY, -1)

    # Initialize the scrollable areas/pads for the kernel and command outputs.
    global KdpKernelOutputWindow, KdpCommandOutputWindow
    KdpKernelOutputWindow = curses.newpad(KdpKernelOutputMaxLines, 1)
    KdpCommandOutputWindow = curses.newpad(KdpCommandOutputMaxLines, 1)
    KdpRefreshInterface()

    # For now, signal to the user that no input can be given (as we're waiting for a connection).
    KdChangeInputMessage("waiting for connection...")

#--------------------------------------------------------------------------------------------------
# PURPOSE:
#     This function shutdowns the debugger interface, which needs to happen before the program
#     finishes execution.
#
# PARAMETERS:
#     None.
#
# RETURN VALUE:
#     None.
#--------------------------------------------------------------------------------------------------
def KdpShutdownInterface() -> None:
    curses.endwin()

#--------------------------------------------------------------------------------------------------
# PURPOSE:
#     This function handles scrolling a given area a certain amount of lines up/down.
#
# PARAMETERS:
#     Window - Which window to scroll.
#     Height - Vertical size of the window.
#     XOffset - Horizontal offset for the refresh operation.
#     YOffset - Vertical offset for the refresh operation.
#     XMax - Maximum (inclusive) horizontal extent for the refresh operation.
#     YMax - Maximum (inclusive) vertical extent for the refresh operation.
#     CurrentPosition - Current selected line in the window.
#     Delta - How much to move up/down.
#
# RETURN VALUE:
#     New current position/line in the window.
#--------------------------------------------------------------------------------------------------
def KdpScrollWindow(
    Window: curses.window,
    Height: int,
    RefreshCoords: tuple[int, int, int, int],
    CurrentPosition: int,
    Delta: int) -> int:
    CurrentLine, _ = Window.getyx()
    YBegin, XBegin, YEnd, XEnd = RefreshCoords

    if Delta < 0:
        CurrentPosition = max(0, CurrentPosition + Delta)
    else:
        MaxPosition = max(0, CurrentLine - Height + 2)
        CurrentPosition = min(MaxPosition, CurrentPosition + Delta)

    Window.refresh(CurrentPosition, 0, YBegin, XBegin, YEnd, XEnd)

    return CurrentPosition

#--------------------------------------------------------------------------------------------------
# PURPOSE:
#     This function handles scrolling when a keyboard or mouse input happens.
#
# PARAMETERS:
#     Key - What key was pressed.
#
# RETURN VALUE:
#     True if we handled anything, False otherwise.
#--------------------------------------------------------------------------------------------------
def KdpUpdateScroll(Key: int) -> bool:
    global KdpSelectedOutput
    global KdpScreenWidth, KdpScreenHeight
    global KdpKernelOutputWindow, KdpKernelOutputCurrentPosition
    global KdpKernelOutputWidth, KdpKernelOutputHeight
    global KdpCommandOutputWindow, KdpCommandOutputCurrentPosition
    global KdpCommandOutputWidth, KdpCommandOutputHeight

    # Tab swaps between the kernel and command outputs.
    if Key == ord("\t"):
        if KdpSelectedOutput == KD_DEST_COMMAND:
            KdpSelectedOutput = KD_DEST_KERNEL
        else:
            KdpSelectedOutput = KD_DEST_COMMAND

        SelectedAttribute = curses.A_BOLD | curses.color_pair(KDP_COLOR_BLUE)
        UnselectedAttribute = curses.A_BOLD

        KdpKernelOutputContainer.addstr(
            0,
            2,
            " Kernel Output ",
            SelectedAttribute if KdpSelectedOutput == KD_DEST_KERNEL else UnselectedAttribute)
        KdpKernelOutputContainer.refresh()

        KdpCommandOutputContainer.addstr(
            0,
            2,
            " Command Output ",
            SelectedAttribute if KdpSelectedOutput == KD_DEST_COMMAND else UnselectedAttribute)
        KdpCommandOutputContainer.refresh()

        return True

    # The page size is simply always the real size (height) of the selected window's content.
    if KdpSelectedOutput == KD_DEST_KERNEL:
        PageSize = KdpKernelOutputHeight - 2
    else:
        PageSize = KdpCommandOutputHeight - 2

    # All other valid operations are related to scrolling (keyboard up/down, and page up/down;
    # the if/elses are in this order too).
    Delta = 0
    if Key == curses.KEY_UP:
        Delta = -1
    elif Key == curses.KEY_DOWN:
        Delta = 1
    elif Key == curses.KEY_PPAGE:
        Delta = -PageSize
    elif Key == curses.KEY_NPAGE:
        Delta = PageSize
    else:
        return False

    if KdpSelectedOutput == KD_DEST_KERNEL:
        KdpKernelOutputCurrentPosition = KdpScrollWindow(
            KdpKernelOutputWindow,
            KdpKernelOutputHeight,
            KdpKernelOutputRefreshCoords,
            KdpKernelOutputCurrentPosition,
            Delta)
    else:
        KdpCommandOutputCurrentPosition = KdpScrollWindow(
            KdpCommandOutputWindow,
            KdpCommandOutputHeight,
            KdpCommandOutputRefreshCoords,
            KdpCommandOutputCurrentPosition,
            Delta)

    return True

#--------------------------------------------------------------------------------------------------
# PURPOSE:
#     This function outputs a message into the correct region of the screen, converting any ANSI
#     escape sequences into proper (n)curses attributes.
#
# PARAMETERS:
#     Destination - Where the message should be sent.
#     Message - What should be displayed.
#
# RETURN VALUE:
#     None.
#--------------------------------------------------------------------------------------------------
def KdpPrint(Destination: int, Message: str) -> None:
    global KdpScreenWidth, KdpScreenHeight
    global KdpKernelOutputWindow, KdpKernelOutputCurrentPosition
    global KdpKernelOutputWidth, KdpKernelOutputHeight
    global KdpCommandOutputWindow, KdpCommandOutputCurrentPosition
    global KdpCommandOutputWidth, KdpCommandOutputHeight

    # Tokenize the incoming message using the supported/expected attributes.
    MessageParts = re.split(
        f"({re.escape(KDP_ANSI_FG_RED)}|" +
        f"{re.escape(KDP_ANSI_FG_GREEN)}|" +
        f"{re.escape(KDP_ANSI_FG_YELLOW)}|" +
        f"{re.escape(KDP_ANSI_FG_BLUE)}|" +
        f"{re.escape(KDP_ANSI_RESET)})",
        Message)

    # And print while handling attribute changes that will probably happen.
    CurrentAttribute = curses.A_NORMAL
    for Part in MessageParts:
        if Part == KDP_ANSI_FG_RED:
            CurrentAttribute = curses.color_pair(KDP_COLOR_RED)
        elif Part == KDP_ANSI_FG_GREEN:
            CurrentAttribute = curses.color_pair(KDP_COLOR_GREEN)
        elif Part == KDP_ANSI_FG_YELLOW:
            CurrentAttribute = curses.color_pair(KDP_COLOR_YELLOW)
        elif Part == KDP_ANSI_FG_BLUE:
            CurrentAttribute = curses.color_pair(KDP_COLOR_BLUE)
        elif Part == KDP_ANSI_RESET:
            CurrentAttribute = curses.A_NORMAL
        elif Part:
            if Destination == KD_DEST_KERNEL:
                KdpKernelOutputWindow.addstr(Part, CurrentAttribute)
                KdpKernelOutputBuffer.append((Part, CurrentAttribute))
            else:
                KdpCommandOutputWindow.addstr(Part, CurrentAttribute)
                KdpCommandOutputBuffer.append((Part, CurrentAttribute))

    if Destination == KD_DEST_KERNEL:
        KdpKernelOutputCurrentPosition = KdpScrollWindow(
            KdpKernelOutputWindow,
            KdpKernelOutputHeight,
            KdpKernelOutputRefreshCoords,
            KdpKernelOutputCurrentPosition,
            1)
    else:
        KdpCommandOutputCurrentPosition = KdpScrollWindow(
            KdpCommandOutputWindow,
            KdpCommandOutputHeight,
            KdpCommandOutputRefreshCoords,
            KdpCommandOutputCurrentPosition,
            1)

#--------------------------------------------------------------------------------------------------
# PURPOSE:
#     This function outputs a prefixed message into the correct region of the screen.
#
# PARAMETERS:
#     Destination - Where the message should be sent.
#     Type - Which kind of message this is (this defines the prefix we prepend to the message).
#     Message - Actual message to be printed.
#
# RETURN VALUE:
#     None.
#--------------------------------------------------------------------------------------------------
def KdPrint(Destination: int, Type: int, Message: str) -> None:
    # Short circuit for KD_TYPE_NONE, as that one just prints without any prefix.
    if Type == KD_TYPE_NONE:
        KdpPrint(Destination, Message)
        return

    # Extract out the prefix string + color out of the type.
    ColorPrefix: str = None
    Prefix: str = None
    if Type == KD_TYPE_ERROR:
        ColorPrefix = KDP_ANSI_FG_RED
        Prefix = "* error: "
    elif Type == KD_TYPE_TRACE:
        ColorPrefix = KDP_ANSI_FG_GREEN
        Prefix = "* trace: "
    elif Type == KD_TYPE_DEBUG:
        ColorPrefix = KDP_ANSI_FG_YELLOW
        Prefix = "* debug: "
    else:
        ColorPrefix = KDP_ANSI_FG_BLUE
        Prefix = "* info: "

    # Print the prefix on the correct color + the message afterwards (on the default color).
    KdpPrint(Destination, f"{ColorPrefix}{Prefix}{KDP_ANSI_RESET}{Message}")

#--------------------------------------------------------------------------------------------------
# PURPOSE:
#     This function changes the message on the input window. Don't use this for printing the input
#     prompt, as the attribute change we do is probably not okay in those cases!
#
# PARAMETERS:
#     Message - What to show.
#
# RETURN VALUE:
#     None.
#--------------------------------------------------------------------------------------------------
def KdChangeInputMessage(Message: str) -> None:
    global KdpInputWindow
    Attribute = curses.A_ITALIC | curses.color_pair(KDP_COLOR_GREY)
    KdpInputWindow.clear()
    KdpInputWindow.addstr(Message, Attribute)
    KdpInputWindow.refresh()

#--------------------------------------------------------------------------------------------------
# PURPOSE:
#     This function handles updating the input line, and any other keyboard/mouse-related events.
#
# PARAMETERS:
#     PromptState - true/false depending on if the prompt is already on the screen.
#     AllowInput - true/false depending on if we should show the prompt and actually read in the
#                  input line.
#     InputLine - Current input line state.
#     Prompt - What prompt to show if none is already on the screen.
#
# RETURN VALUE:
#     A tuple, containg the new PromptState, an updated input line, and if we finished reading the
#     input line.
#--------------------------------------------------------------------------------------------------
def KdReadInput(
    PromptState: bool,
    AllowInput: bool,
    InputLine: str,
    Prompt: str) -> tuple[bool, str, bool]:
    global KdpInputWindow

    # Update (print) the prompt if this is our first run (or we just finished reading an
    # input line).
    if not PromptState and AllowInput:
        curses.curs_set(2)
        KdpInputWindow.clear()
        KdpInputWindow.addstr(Prompt)
        KdpInputWindow.refresh()
        PromptState = True

    # Don't bother if we can't immediataly read an input character (the caller probably has other
    # stuff to handle too).
    Key = KdpInputWindow.getch()
    if Key == -1:
        return (PromptState, InputLine, False)

    # Handle resizing (as we need to adjust the sizing of the kernel and command output areas).
    if Key == curses.KEY_RESIZE:
        CurrentPrompt = KdpInputWindow.instr(0, 0).decode().strip()
        KdpRefreshInterface()
        if AllowInput:
            KdpInputWindow.clear()
            KdpInputWindow.addstr(CurrentPrompt)
            KdpInputWindow.refresh()
        else:
            KdChangeInputMessage(CurrentPrompt)
        return (PromptState, InputLine, False)

    # Also try handling scrolling (this happens even if input is disabled).
    if KdpUpdateScroll(Key) or not AllowInput:
        # Just make sure to return the cursor to the input line if KdpUpdateScroll did anything.
        if AllowInput:
            KdpInputWindow.refresh()
        return (PromptState, InputLine, False)

    # Enter signifies that we're finished (we can finally return True for the last one, and swap
    # the PromptState to false, as we need to clear the line again).
    if Key in (curses.KEY_ENTER, ord("\n")):
        return (False, InputLine, True)

    # Is this the right way of handling backspace?
    if Key in (curses.KEY_BACKSPACE, 127, 8):
        if len(InputLine):
            InputLine = InputLine[:-1]
            KdpInputWindow.addstr("\b")
            KdpInputWindow.delch()
            KdpInputWindow.refresh()
        return (PromptState, InputLine, False)

    if 32 <= Key < 127:
        InputLine += chr(Key)
        KdpInputWindow.addch(Key)
        KdpInputWindow.refresh()

    return (PromptState, InputLine, False)
