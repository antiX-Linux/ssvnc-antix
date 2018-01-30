/*
 *  Copyright (C) 2002-2003 Constantin Kaplinsky.  All Rights Reserved.
 *  Copyright (C) 1999 AT&T Laboratories Cambridge.  All Rights Reserved.
 *
 *  This is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This software is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this software; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,
 *  USA.
 */

/*
 * argsresources.c - deal with command-line args and resources.
 */

#include "vncviewer.h"

/*
 * fallback_resources - these are used if there is no app-defaults file
 * installed in one of the standard places.
 */

char *fallback_resources[] = {

#if (defined(__MACH__) && defined(__APPLE__))
  "Ssvnc.title: SSVNC: %s - Press F7 for Menu",
#else
  "Ssvnc.title: SSVNC: %s - Press F8 for Menu",
#endif

  "Ssvnc.translations:\
    <Enter>: SelectionToVNC()\\n\
    <Leave>: SelectionFromVNC()",

  "*form.background: black",

  "*viewport.allowHoriz: True",
  "*viewport.allowVert: True",
  "*viewport.useBottom: True",
  "*viewport.useRight: True",
  "*viewport*Scrollbar*thumb: None",

           "*viewport.horizontal.height:   6 ",
              "*viewport.vertical.width:   6 ",
  "ssvnc*viewport.horizontal.height:   6 ",
     "ssvnc*viewport.vertical.width:   6 ",

  "*viewport.horizontal.translations: #override\\n\
     <KeyPress>Right:  StartScroll(Forward)\\n\
     <KeyRelease>Right:  NotifyScroll(FullLength) EndScroll()\\n\
     <KeyPress>Left:  StartScroll(Backward)\\n\
     <KeyRelease>Left:  NotifyScroll(FullLength) EndScroll()\\n\
     <KeyPress>Next:  StartScroll(Forward)\\n\
     <KeyRelease>Next:  NotifyScroll(FullLength) EndScroll()\\n\
     <KeyPress>Prior:  StartScroll(Backward)\\n\
     <KeyRelease>Prior:  NotifyScroll(FullLength) EndScroll()\\n\
     <KeyPress>z:  StartScroll(Forward)\\n\
     <KeyRelease>z:  NotifyScroll(FullLength) EndScroll()\\n\
     <KeyPress>a:  StartScroll(Backward)\\n\
     <KeyRelease>a:  NotifyScroll(FullLength) EndScroll()\\n\
     <KeyPress>f:  StartScroll(Forward)\\n\
     <KeyRelease>f:  NotifyScroll(FullLength) EndScroll()\\n\
     <KeyPress>b:  StartScroll(Backward)\\n\
     <KeyRelease>b:  NotifyScroll(FullLength) EndScroll()\\n\
     <KeyPress>Down:  StartScroll(Forward)\\n\
     <KeyRelease>Down:  NotifyScroll(FullLength) EndScroll()\\n\
     <KeyPress>Up:  StartScroll(Backward)\\n\
     <KeyRelease>Up:  NotifyScroll(FullLength) EndScroll()",

  "*viewport.vertical.translations: #override\\n\
     <KeyPress>Down:  StartScroll(Forward)\\n\
     <KeyRelease>Down:  NotifyScroll(FullLength) EndScroll()\\n\
     <KeyPress>Up:  StartScroll(Backward)\\n\
     <KeyRelease>Up:  NotifyScroll(FullLength) EndScroll()\\n\
     <KeyPress>Next:  StartScroll(Forward)\\n\
     <KeyRelease>Next:  NotifyScroll(FullLength) EndScroll()\\n\
     <KeyPress>Prior:  StartScroll(Backward)\\n\
     <KeyRelease>Prior:  NotifyScroll(FullLength) EndScroll()\\n\
     <KeyPress>z:  StartScroll(Forward)\\n\
     <KeyRelease>z:  NotifyScroll(FullLength) EndScroll()\\n\
     <KeyPress>a:  StartScroll(Backward)\\n\
     <KeyRelease>a:  NotifyScroll(FullLength) EndScroll()\\n\
     <KeyPress>f:  StartScroll(Forward)\\n\
     <KeyRelease>f:  NotifyScroll(FullLength) EndScroll()\\n\
     <KeyPress>b:  StartScroll(Backward)\\n\
     <KeyRelease>b:  NotifyScroll(FullLength) EndScroll()\\n\
     <KeyPress>Right:  StartScroll(Forward)\\n\
     <KeyRelease>Right:  NotifyScroll(FullLength) EndScroll()\\n\
     <KeyPress>Left:  StartScroll(Backward)\\n\
     <KeyRelease>Left:  NotifyScroll(FullLength) EndScroll()",

#if (defined(__MACH__) && defined(__APPLE__))
  "*desktop.baseTranslations:\
     <KeyPress>F7:  ShowPopup()\\n\
     <KeyRelease>F7:  Noop()\\n\
     <KeyPress>F8:  ShowPopup()\\n\
     <KeyRelease>F8:  Noop()\\n\
     <KeyPress>F9:  ToggleFullScreen()\\n\
     <KeyRelease>F9:  Noop()\\n\
     <ButtonPress>: SendRFBEvent()\\n\
     <ButtonRelease>: SendRFBEvent()\\n\
     <Motion>: SendRFBEvent()\\n\
     <KeyPress>: SendRFBEvent()\\n\
     <KeyRelease>: SendRFBEvent()",
#else
  "*desktop.baseTranslations:\
     <KeyPress>F8:  ShowPopup()\\n\
     <KeyRelease>F8:  Noop()\\n\
     <KeyPress>F9:  ToggleFullScreen()\\n\
     <KeyRelease>F9:  Noop()\\n\
     <ButtonPress>: SendRFBEvent()\\n\
     <ButtonRelease>: SendRFBEvent()\\n\
     <Motion>: SendRFBEvent()\\n\
     <KeyPress>: SendRFBEvent()\\n\
     <KeyRelease>: SendRFBEvent()",
#endif

  "*serverDialog.dialog.label: VNC server:",
  "*serverDialog.dialog.value:",
  "*serverDialog.dialog.value.width: 150",
  "*serverDialog.dialog.value.translations: #override\\n\
     <Key>Return: ServerDialogDone()",

  "*userDialog.dialog.label: SSVNC: Enter Username",
  "*userDialog.dialog.value:",
  "*userDialog.dialog.value.width: 150",
  "*userDialog.dialog.value.translations: #override\\n\
     <Key>Return: UserDialogDone()",

  "*scaleDialog.dialog.label: Scale: Enter 'none' (same as '1' or '1.0'),\\na geometry WxH (e.g. 1280x1024), or\\na fraction (e.g. 0.75 or 3/4).\\nUse 'fit' for full screen size.\\nUse 'auto' to match window size.\\nCurrent value:",
  "*scaleDialog.dialog.value:",
  "*scaleDialog.dialog.value.translations: #override\\n\
     <KeyRelease>Return: ScaleDialogDone()",

  "*escapeDialog.dialog.label: Escape Keys:  Enter a comma separated list of modifier keys to be the\\n"
                              "'escape sequence'.  When these keys are held down, the next keystroke is\\n"
                              "interpreted locally to invoke a special action instead of being sent to\\n"
                              "the remote VNC server.  In other words, a set of 'Hot Keys'.\\n"
                              "\\n"
                              "To enable or disable this, click on 'Escape Keys: Toggle' in the Popup.\\n"
                              "\\n"
                              "Here is the list of hot-key mappings to special actions:\\n"
                              "\\n"
                              "   r: refresh desktop  b: toggle bell   c: toggle full-color\\n"
                              "   f: file transfer    x: x11cursor     z: toggle Tight/ZRLE\\n"
                              "   l: full screen      g: graball       e: escape keys dialog\\n"
                              "   s: scale dialog     +: scale up (=)  -: scale down (_)\\n"
                              "   t: text chat                         a: alphablend cursor\\n"
                              "   V: toggle viewonly  Q: quit viewer   1 2 3 4 5 6: UltraVNC scale 1/n\\n"
                              "\\n"
                              "   Arrow keys:         pan the viewport about 10% for each keypress.\\n"
                              "   PageUp / PageDown:  pan the viewport by a screenful vertically.\\n"
                              "   Home   / End:       pan the viewport by a screenful horizontally.\\n"
                              "   KeyPad Arrow keys:  pan the viewport by 1 pixel for each keypress.\\n"
                              "   Dragging the Mouse with Button1 pressed also pans the viewport.\\n"
                              "   Clicking Mouse Button3 brings up the Popup Menu.\\n"
                              "\\n"
                              "The above mappings are *always* active in ViewOnly mode, unless you set the\\n"
                              "Escape Keys value to 'never'.\\n"
                              "\\n"
                              "x11vnc -appshare hot-keys:  x11vnc has a simple application sharing mode\\n"
                              "that enables the viewer-side to move, resize, or raise the remote toplevel\\n"
                              "windows.  To enable it, hold down Shift + the Escape Keys and press these:\\n"
                              "\\n"
                              "   Arrow keys:              move the remote window around in its desktop.\\n"
                              "   PageUp/PageDn/Home/End:  resize the remote window.\\n"
                              "   +/-                      raise or lower the remote window.\\n"
                              "   M or Button1 move win to local position;  D or Button3: delete remote win.\\n"
                              "\\n"
                              "If the Escape Keys value below is set to 'default' then a fixed list of\\n"
                              "modifier keys is used.  For Unix it is: Alt_L,Super_L and for MacOSX it is\\n"
                              "Control_L,Meta_L.  Note: the Super_L key usually has a Windows(TM) Flag.\\n"
                              "Also note the _L and _R mean the key is on the LEFT or RIGHT side of keyboard.\\n"
                              "\\n"
                              "On Unix   the default is Alt and Windows keys on Left side of keyboard.\\n"
                              "On MacOSX the default is Control and Command keys on Left side of keyboard.\\n"
                              "\\n"
                              "Example: Press and hold the Alt and Windows keys on the LEFT side of the\\n"
                              "keyboard and then press 'c' to toggle the full-color state.  Or press 't'\\n"
                              "to toggle the ultravnc Text Chat window, etc.\\n"
                              "\\n"
                              "To use something besides the default, supply a comma separated list (or a\\n"
                              "single one) from: Shift_L Shift_R Control_L Control_R Alt_L Alt_R Meta_L\\n"
                              "Meta_R Super_L Super_R Hyper_L Hyper_R or Mode_switch.\\n"
                              "\\n"
                              "Current Escape Keys Value:",
  "*escapeDialog.dialog.value:",
  "*escapeDialog.dialog.value.width: 280",
  "*escapeDialog.dialog.value.translations: #override\\n\
     <KeyRelease>Return: EscapeDialogDone()",

  "*ycropDialog.dialog.label: Y Crop (max-height in pixels):",
  "*ycropDialog.dialog.value:",
  "*ycropDialog.dialog.value.translations: #override\\n\
     <KeyRelease>Return: YCropDialogDone()",

  "*scbarDialog.dialog.label: Scroll Bars width:",
  "*scbarDialog.dialog.value:",
  "*scbarDialog.dialog.value.translations: #override\\n\
     <KeyRelease>Return: ScbarDialogDone()",

  "*scaleNDialog.dialog.label: Integer n for 1/n server scaling:",
  "*scaleNDialog.dialog.value:",
  "*scaleNDialog.dialog.value.translations: #override\\n\
     <KeyRelease>Return: ScaleNDialogDone()",

  "*passwordDialog.dialog.label: SSVNC: Enter Password",
  "*passwordDialog.dialog.value:",
  "*passwordDialog.dialog.value.width: 150",
  "*passwordDialog.dialog.value.AsciiSink.echo: False",
  "*passwordDialog.dialog.value.translations: #override\\n\
     <Key>Return: PasswordDialogDone()",

  "*popup.title: SSVNC popup",
  "*popup*background: grey",
  "*popup*font_old: -*-helvetica-bold-r-*-*-16-*-*-*-*-*-*-*",
  "*popup*font: -*-helvetica-medium-r-*-*-12-*-*-*-*-*-*-*",
  "*popup.buttonForm*.Command.borderWidth: 0",
  "*popup.buttonForm*.Toggle.borderWidth: 0",

  "*scaleN.title: 1/n scale",
  "*scaleN*background: grey",
  "*scaleN*font: -*-helvetica-medium-r-*-*-12-*-*-*-*-*-*-*",
  "*scaleN.buttonForm.Command.borderWidth: 0",
  "*scaleN.buttonForm.Toggle.borderWidth: 0",

  "*turboVNC.title: TurboVNC",
  "*turboVNC*background: grey",
  "*turboVNC*font: -*-helvetica-medium-r-*-*-12-*-*-*-*-*-*-*",
  "*turboVNC.buttonForm.Command.borderWidth: 0",
  "*turboVNC.buttonForm.Toggle.borderWidth: 0",

  "*quality.title: quality",
  "*quality*background: grey",
  "*quality*font: -*-helvetica-medium-r-*-*-12-*-*-*-*-*-*-*",
  "*quality.buttonForm.Command.borderWidth: 0",
  "*quality.buttonForm.Toggle.borderWidth: 0",

  "*compress.title: compress",
  "*compress*background: grey",
  "*compress*font: -*-helvetica-medium-r-*-*-12-*-*-*-*-*-*-*",
  "*compress.buttonForm.Command.borderWidth: 0",
  "*compress.buttonForm.Toggle.borderWidth: 0",

  "*popup.translations: #override <Message>WM_PROTOCOLS: HidePopup()",
  "*popup.buttonForm.translations: #override\\n\
     <KeyPress>: SendRFBEvent() HidePopup()",

  "*popupButtonCount: 44",
  "*popupButtonBreak: 22",

  "*popup*button1.label: Dismiss popup",
  "*popup*button1.translations: #override\\n\
     <Btn1Down>,<Btn1Up>: HidePopup()",

  "*popup*button2.label: Quit viewer",
  "*popup*button2.translations: #override\\n\
     <Btn1Down>,<Btn1Up>: Quit()",

  "*popup*button3.label: Full screen  (also F9)",
  "*popup*button3.type: toggle",
  "*popup*button3.translations: #override\\n\
     <Visible>: SetFullScreenState()\\n\
     <Btn1Down>,<Btn1Up>: toggle() ToggleFullScreen() HidePopup()",

  "*popup*button4.label: Clipboard: local -> remote",
  "*popup*button4.translations: #override\\n\
     <Btn1Down>,<Btn1Up>: SelectionToVNC(always) HidePopup()",

  "*popup*button5.label: Clipboard: local <- remote",
  "*popup*button5.translations: #override\\n\
     <Btn1Down>,<Btn1Up>: SelectionFromVNC(always) HidePopup()",

  "*popup*button6.label: Request refresh",
  "*popup*button6.translations: #override\\n\
     <Btn1Down>,<Btn1Up>: SendRFBEvent(fbupdate) HidePopup()",

  "*popup*button7.label: Send ctrl-alt-del",
  "*popup*button7.translations: #override\\n\
     <Btn1Down>,<Btn1Up>: SendRFBEvent(keydown,Control_L)\
     SendRFBEvent(keydown,Alt_L)\
     SendRFBEvent(key,Delete)\
     SendRFBEvent(keyup,Alt_L)\
     SendRFBEvent(keyup,Control_L)\
     HidePopup()",

#if (defined(__MACH__) && defined(__APPLE__))
  "*popup*button8.label: Send F7",
  "*popup*button8.translations: #override\\n\
     <Btn1Down>,<Btn1Up>: SendRFBEvent(key,F7) HidePopup()",
#else
  "*popup*button8.label: Send F8",
  "*popup*button8.translations: #override\\n\
     <Btn1Down>,<Btn1Up>: SendRFBEvent(key,F8) HidePopup()",
#endif

  "*popup*button9.label: Send F9",
  "*popup*button9.translations: #override\\n\
     <Btn1Down>,<Btn1Up>: SendRFBEvent(key,F9) HidePopup()",

  "*popup*button10.label: ViewOnly",
  "*popup*button10.type: toggle",
  "*popup*button10.translations: #override\\n\
     <Visible>: SetViewOnlyState()\\n\
     <Btn1Down>,<Btn1Up>: toggle() ToggleViewOnly() HidePopup()",

  "*popup*button11.label: Disable Bell",
  "*popup*button11.type: toggle",
  "*popup*button11.translations: #override\\n\
     <Visible>: SetBellState()\\n\
     <Btn1Down>,<Btn1Up>: toggle() ToggleBell() HidePopup()",

  "*popup*button12.label: Cursor Shape",
  "*popup*button12.type: toggle",
  "*popup*button12.translations: #override\\n\
     <Visible>: SetCursorShapeState()\\n\
     <Btn1Down>,<Btn1Up>: toggle() ToggleCursorShape() HidePopup()",

  "*popup*button13.label: X11 Cursor",
  "*popup*button13.type: toggle",
  "*popup*button13.translations: #override\\n\
     <Visible>: SetX11CursorState()\\n\
     <Btn1Down>,<Btn1Up>: toggle() ToggleX11Cursor() HidePopup()",

  "*popup*button14.label: Cursor Alphablend",
  "*popup*button14.type: toggle",
  "*popup*button14.translations: #override\\n\
     <Visible>: SetCursorAlphaState()\\n\
     <Btn1Down>,<Btn1Up>: toggle() ToggleCursorAlpha() HidePopup()",

  "*popup*button15.label: Toggle Tight/Hextile",
  "*popup*button15.type: toggle",
  "*popup*button15.translations: #override\\n\
     <Visible>: SetHextileState()\\n\
     <Btn1Down>,<Btn1Up>: toggle() ToggleTightHextile() HidePopup()",

  "*popup*button16.label: Toggle Tight/ZRLE",
  "*popup*button16.type: toggle",
  "*popup*button16.translations: #override\\n\
     <Visible>: SetZRLEState()\\n\
     <Btn1Down>,<Btn1Up>: toggle() ToggleTightZRLE() HidePopup()",

  "*popup*button17.label: Toggle ZRLE/ZYWRLE",
  "*popup*button17.type: toggle",
  "*popup*button17.translations: #override\\n\
     <Visible>: SetZYWRLEState()\\n\
     <Btn1Down>,<Btn1Up>: toggle() ToggleZRLEZYWRLE() HidePopup()",

  "*popup*button18.label: Quality Level",
  "*popup*button18.translations: #override\\n\
     <Btn1Down>,<Btn1Up>: HidePopup() ShowQuality()",

  "*popup*button19.label: Compress Level",
  "*popup*button19.translations: #override\\n\
     <Btn1Down>,<Btn1Up>: HidePopup() ShowCompress()",

  "*popup*button20.label: Disable JPEG",
  "*popup*button20.type: toggle",
  "*popup*button20.translations: #override\\n\
     <Visible>: SetNOJPEGState()\\n\
     <Btn1Down>,<Btn1Up>: toggle() ToggleJPEG() HidePopup()",

  "*popup*button21.label: TurboVNC Settings",
  "*popup*button21.translations: #override\\n\
     <Btn1Down>,<Btn1Up>: HidePopup() ShowTurboVNC()",

  "*popup*button22.label: Pipeline Updates",
  "*popup*button22.type: toggle",
  "*popup*button22.translations: #override\\n\
     <Visible>: SetPipelineUpdates()\\n\
     <Btn1Down>,<Btn1Up>: toggle() TogglePipelineUpdates() HidePopup()",

  "*popup*button23.label: Full Color",
  "*popup*button23.type: toggle",
  "*popup*button23.translations: #override\\n\
     <Visible>: SetFullColorState()\\n\
     <Btn1Down>,<Btn1Up>: toggle() ToggleFullColor() HidePopup()",

  "*popup*button24.label: Grey Scale (16 & 8-bpp)",
  "*popup*button24.type: toggle",
  "*popup*button24.translations: #override\\n\
     <Visible>: SetGreyScaleState()\\n\
     <Btn1Down>,<Btn1Up>: toggle() ToggleGreyScale() HidePopup()",

  "*popup*button25.label: 16 bit color (BGR565)",
  "*popup*button25.type: toggle",
  "*popup*button25.translations: #override\\n\
     <Visible>: Set16bppState()\\n\
     <Btn1Down>,<Btn1Up>: toggle() Toggle16bpp() HidePopup()",

  "*popup*button26.label: 8   bit color (BGR233)",
  "*popup*button26.type: toggle",
  "*popup*button26.translations: #override\\n\
     <Visible>: Set8bppState()\\n\
     <Btn1Down>,<Btn1Up>: toggle() Toggle8bpp() HidePopup()",

  "*popup*button27.label: -     256 colors",
  "*popup*button27.type: toggle",
  "*popup*button27.translations: #override\\n\
     <Visible>: Set256ColorsState()\\n\
     <Btn1Down>,<Btn1Up>: toggle() Toggle256Colors() HidePopup()",

  "*popup*button28.label: -       64 colors",
  "*popup*button28.type: toggle",
  "*popup*button28.translations: #override\\n\
     <Visible>: Set64ColorsState()\\n\
     <Btn1Down>,<Btn1Up>: toggle() Toggle64Colors() HidePopup()",

  "*popup*button29.label: -         8 colors",
  "*popup*button29.type: toggle",
  "*popup*button29.translations: #override\\n\
     <Visible>: Set8ColorsState()\\n\
     <Btn1Down>,<Btn1Up>: toggle() Toggle8Colors() HidePopup()",

  "*popup*button30.label: Scale Viewer",
  "*popup*button30.translations: #override\\n\
     <Btn1Down>,<Btn1Up>: HidePopup() SetScale()",

  "*popup*button31.label: Escape Keys: Toggle",
  "*popup*button31.type: toggle",
  "*popup*button31.translations: #override\\n\
     <Visible>: SetEscapeKeysState()\\n\
     <Btn1Down>, <Btn1Up>: toggle() ToggleEscapeActive() HidePopup()",

  "*popup*button32.label: Escape Keys: Help+Set",
  "*popup*button32.translations: #override\\n\
      <Btn1Down>, <Btn1Up>: HidePopup() SetEscapeKeys()",

  "*popup*button33.label: Set Y Crop (y-max)",
  "*popup*button33.translations: #override\\n\
     <Btn1Down>,<Btn1Up>: HidePopup() SetYCrop()",

  "*popup*button34.label: Set Scrollbar Width",
  "*popup*button34.translations: #override\\n\
     <Btn1Down>,<Btn1Up>: HidePopup() SetScbar()",

  "*popup*button35.label: XGrabServer",
  "*popup*button35.type: toggle",
  "*popup*button35.translations: #override\\n\
     <Visible>: SetXGrabState()\\n\
     <Btn1Down>,<Btn1Up>: toggle() ToggleXGrab() HidePopup()",

  "*popup*button36.label: UltraVNC Extensions:",
  "*popup*button36.translations: #override\\n\
     <Btn1Down>,<Btn1Up>: HidePopup()",

  "*popup*button37.label: - Set 1/n Server Scale",
  "*popup*button37.translations: #override\\n\
     <Btn1Down>,<Btn1Up>: HidePopup() ShowScaleN()",

  "*popup*button38.label: - Text Chat",
  "*popup*button38.type: toggle",
  "*popup*button38.translations: #override\\n\
     <Visible>: SetTextChatState()\\n\
     <Btn1Down>,<Btn1Up>: toggle() ToggleTextChat() HidePopup()",

  "*popup*button39.label: - File Transfer",
  "*popup*button39.type: toggle",
  "*popup*button39.translations: #override\\n\
     <Visible>: SetFileXferState()\\n\
     <Btn1Down>,<Btn1Up>: toggle() ToggleFileXfer() HidePopup()",

  "*popup*button40.label: - Single Window",
  "*popup*button40.type: toggle",
  "*popup*button40.translations: #override\\n\
     <Visible>: SetSingleWindowState()\\n\
     <Btn1Down>,<Btn1Up>: toggle() ToggleSingleWindow() HidePopup()",

  "*popup*button41.label: - Disable Remote Input",
  "*popup*button41.type: toggle",
  "*popup*button41.translations: #override\\n\
     <Visible>: SetServerInputState()\\n\
     <Btn1Down>,<Btn1Up>: toggle() ToggleServerInput() HidePopup()",

  "*popup*button42.label: Send Clipboard not Primary",
  "*popup*button42.type: toggle",
  "*popup*button42.translations: #override\\n\
     <Visible>: SetSendClipboard()\\n\
     <Btn1Down>,<Btn1Up>: toggle() ToggleSendClipboard() HidePopup()",

  "*popup*button43.label: Send Selection Every time",
  "*popup*button43.type: toggle",
  "*popup*button43.translations: #override\\n\
     <Visible>: SetSendAlways()\\n\
     <Btn1Down>,<Btn1Up>: toggle() ToggleSendAlways() HidePopup()",

  "*popup*button44.label: ",

  "*turboVNC*button0.label: Dismiss",
  "*turboVNC*button0.translations: #override\\n\
     <Btn1Down>,<Btn1Up>: HideTurboVNC()",

  "*turboVNC*button1.label: High Quality (LAN)",
  "*turboVNC*button1.translations: #override\\n\
     <Btn1Down>,<Btn1Up>: SetTurboVNC(1)",

  "*turboVNC*button2.label: Medium Quality",
  "*turboVNC*button2.translations: #override\\n\
     <Btn1Down>,<Btn1Up>: SetTurboVNC(2)",

  "*turboVNC*button3.label: Low Quality (WAN)",
  "*turboVNC*button3.translations: #override\\n\
     <Btn1Down>,<Btn1Up>: SetTurboVNC(3)",

  "*turboVNC*button4.label: Lossless (Gigabit)",
  "*turboVNC*button4.translations: #override\\n\
     <Btn1Down>,<Btn1Up>: SetTurboVNC(4)",

  "*turboVNC*button5.label: Lossless Zlib (WAN)",
  "*turboVNC*button5.translations: #override\\n\
     <Btn1Down>,<Btn1Up>: SetTurboVNC(5)",

  "*turboVNC*button6.label: Subsampling:",

  "*turboVNC*button7.label: -           None",
  "*turboVNC*button7.translations: #override\\n\
     <Btn1Down>,<Btn1Up>: SetTurboVNC(6)",

  "*turboVNC*button8.label: -           2X",
  "*turboVNC*button8.translations: #override\\n\
     <Btn1Down>,<Btn1Up>: SetTurboVNC(7)",

  "*turboVNC*button9.label: -           4X",
  "*turboVNC*button9.translations: #override\\n\
     <Btn1Down>,<Btn1Up>: SetTurboVNC(8)",

  "*turboVNC*button10.label: -          Gray",
  "*turboVNC*button10.translations: #override\\n\
     <Btn1Down>,<Btn1Up>: SetTurboVNC(9)",

  "*turboVNC*button11.label: Lossless Refresh",
  "*turboVNC*button11.translations: #override\\n\
     <Btn1Down>,<Btn1Up>: SetTurboVNC(10)",

  "*turboVNC*button12.label: Lossy Refresh",
  "*turboVNC*button12.translations: #override\\n\
     <Btn1Down>,<Btn1Up>: SendRFBEvent(fbupdate)",

  "*turboVNC*buttonNone.label: Not Compiled with\\nTurboVNC Support.",
  "*turboVNC*buttonNone.translations: #override\\n\
     <Btn1Down>,<Btn1Up>: HideTurboVNC()",

  "*qualLabel.label: JPEG Image Quality:",
  "*qualBar.length: 100",
  "*qualBar.width: 130",
  "*qualBar.orientation: horizontal",
  "*qualBar.translations: #override\\n\
     <Btn1Down>: StartScroll(Continuous) MoveThumb() NotifyThumb()\\n\
     <Btn1Motion>: MoveThumb() NotifyThumb()\\n\
     <Btn3Down>: StartScroll(Continuous) MoveThumb() NotifyThumb()\\n\
     <Btn3Motion>: MoveThumb() NotifyThumb()",
    
  "*qualText.label: 000",

  "*scaleN*button0.label: Dismiss",
  "*scaleN*button0.translations: #override\\n\
     <Btn1Down>,<Btn1Up>: HideScaleN()",

  "*scaleN*button1.label: 1/1",
  "*scaleN*button1.translations: #override\\n\
     <Visible>: SetScaleNState(1)\\n\
     <Btn1Down>,<Btn1Up>: SetScaleN(1) HideScaleN()",

  "*scaleN*button2.label: 1/2",
  "*scaleN*button2.translations: #override\\n\
     <Visible>: SetScaleNState(2)\\n\
     <Btn1Down>,<Btn1Up>: SetScaleN(2) HideScaleN()",

  "*scaleN*button3.label: 1/3",
  "*scaleN*button3.translations: #override\\n\
     <Visible>: SetScaleNState(3)\\n\
     <Btn1Down>,<Btn1Up>: SetScaleN(3) HideScaleN()",

  "*scaleN*button4.label: 1/4",
  "*scaleN*button4.translations: #override\\n\
     <Visible>: SetScaleNState(4)\\n\
     <Btn1Down>,<Btn1Up>: SetScaleN(4) HideScaleN()",

  "*scaleN*button5.label: 1/5",
  "*scaleN*button5.translations: #override\\n\
     <Visible>: SetScaleNState(5)\\n\
     <Btn1Down>,<Btn1Up>: SetScaleN(5) HideScaleN()",

  "*scaleN*button6.label: Other",
  "*scaleN*button6.translations: #override\\n\
     <Visible>: SetScaleNState(6)\\n\
     <Btn1Down>,<Btn1Up>: HideScaleN() DoServerScale()",

  "*quality*buttonD.label: Dismiss",
  "*quality*buttonD.translations: #override\\n\
     <Btn1Down>,<Btn1Up>: HideQuality()",

  "*quality*button0.label: 0",
  "*quality*button0.type: toggle",
  "*quality*button0.translations: #override\\n\
     <Visible>: SetQualityState(0)\\n\
     <Btn1Down>,<Btn1Up>: SetQuality(0) HideQuality()",

  "*quality*button1.label: 1",
  "*quality*button1.type: toggle",
  "*quality*button1.translations: #override\\n\
     <Visible>: SetQualityState(1)\\n\
     <Btn1Down>,<Btn1Up>: SetQuality(1) HideQuality()",

  "*quality*button2.label: 2",
  "*quality*button2.type: toggle",
  "*quality*button2.translations: #override\\n\
     <Visible>: SetQualityState(2)\\n\
     <Btn1Down>,<Btn1Up>: SetQuality(2) HideQuality()",

  "*quality*button3.label: 3",
  "*quality*button3.type: toggle",
  "*quality*button3.translations: #override\\n\
     <Visible>: SetQualityState(3)\\n\
     <Btn1Down>,<Btn1Up>: SetQuality(3) HideQuality()",

  "*quality*button4.label: 4",
  "*quality*button4.type: toggle",
  "*quality*button4.translations: #override\\n\
     <Visible>: SetQualityState(4)\\n\
     <Btn1Down>,<Btn1Up>: SetQuality(4) HideQuality()",

  "*quality*button5.label: 5",
  "*quality*button5.type: toggle",
  "*quality*button5.translations: #override\\n\
     <Visible>: SetQualityState(5)\\n\
     <Btn1Down>,<Btn1Up>: SetQuality(5) HideQuality()",

  "*quality*button6.label: 6",
  "*quality*button6.type: toggle",
  "*quality*button6.translations: #override\\n\
     <Visible>: SetQualityState(6)\\n\
     <Btn1Down>,<Btn1Up>: SetQuality(6) HideQuality()",

  "*quality*button7.label: 7",
  "*quality*button7.type: toggle",
  "*quality*button7.translations: #override\\n\
     <Visible>: SetQualityState(7)\\n\
     <Btn1Down>,<Btn1Up>: SetQuality(7) HideQuality()",

  "*quality*button8.label: 8",
  "*quality*button8.type: toggle",
  "*quality*button8.translations: #override\\n\
     <Visible>: SetQualityState(8)\\n\
     <Btn1Down>,<Btn1Up>: SetQuality(8) HideQuality()",

  "*quality*button9.label: 9",
  "*quality*button9.type: toggle",
  "*quality*button9.translations: #override\\n\
     <Visible>: SetQualityState(9)\\n\
     <Btn1Down>,<Btn1Up>: SetQuality(9) HideQuality()",

  "*compress*buttonD.label: Dismiss",
  "*compress*buttonD.translations: #override\\n\
     <Btn1Down>,<Btn1Up>: HideCompress()",

  "*compress*button0.label: 0",
  "*compress*button0.translations: #override\\n\
     <Visible>: SetCompressState(0)\\n\
     <Btn1Down>,<Btn1Up>: SetCompress(0) HideCompress()",

  "*compress*button1.label: 1",
  "*compress*button1.translations: #override\\n\
     <Visible>: SetCompressState(1)\\n\
     <Btn1Down>,<Btn1Up>: SetCompress(1) HideCompress()",

  "*compress*button2.label: 2",
  "*compress*button2.translations: #override\\n\
     <Visible>: SetCompressState(2)\\n\
     <Btn1Down>,<Btn1Up>: SetCompress(2) HideCompress()",

  "*compress*button3.label: 3",
  "*compress*button3.translations: #override\\n\
     <Visible>: SetCompressState(3)\\n\
     <Btn1Down>,<Btn1Up>: SetCompress(3) HideCompress()",

  "*compress*button4.label: 4",
  "*compress*button4.translations: #override\\n\
     <Visible>: SetCompressState(4)\\n\
     <Btn1Down>,<Btn1Up>: SetCompress(4) HideCompress()",

  "*compress*button5.label: 5",
  "*compress*button5.translations: #override\\n\
     <Visible>: SetCompressState(5)\\n\
     <Btn1Down>,<Btn1Up>: SetCompress(5) HideCompress()",

  "*compress*button6.label: 6",
  "*compress*button6.translations: #override\\n\
     <Visible>: SetCompressState(6)\\n\
     <Btn1Down>,<Btn1Up>: SetCompress(6) HideCompress()",

  "*compress*button7.label: 7",
  "*compress*button7.translations: #override\\n\
     <Visible>: SetCompressState(7)\\n\
     <Btn1Down>,<Btn1Up>: SetCompress(7) HideCompress()",

  "*compress*button8.label: 8",
  "*compress*button8.translations: #override\\n\
     <Visible>: SetCompressState(8)\\n\
     <Btn1Down>,<Btn1Up>: SetCompress(8) HideCompress()",

  "*compress*button9.label: 9",
  "*compress*button9.translations: #override\\n\
     <Visible>: SetCompressState(9)\\n\
     <Btn1Down>,<Btn1Up>: SetCompress(9) HideCompress()",

  NULL
};


/*
 * vncServerHost and vncServerPort are set either from the command line or
 * from a dialog box.
 */

char vncServerHost[1024];
int vncServerPort = 0;


/*
 * appData is our application-specific data which can be set by the user with
 * application resource specs.  The AppData structure is defined in the header
 * file.
 */

AppData appData;
AppData appDataNew;

static XtResource appDataResourceList[] = {
  {"shareDesktop", "ShareDesktop", XtRBool, sizeof(Bool),
   XtOffsetOf(AppData, shareDesktop), XtRImmediate, (XtPointer) True},

  {"viewOnly", "ViewOnly", XtRBool, sizeof(Bool),
   XtOffsetOf(AppData, viewOnly), XtRImmediate, (XtPointer) False},

  {"fullScreen", "FullScreen", XtRBool, sizeof(Bool),
   XtOffsetOf(AppData, fullScreen), XtRImmediate, (XtPointer) False},

  {"raiseOnBeep", "RaiseOnBeep", XtRBool, sizeof(Bool),
   XtOffsetOf(AppData, raiseOnBeep), XtRImmediate, (XtPointer) True},

  {"passwordFile", "PasswordFile", XtRString, sizeof(String),
   XtOffsetOf(AppData, passwordFile), XtRImmediate, (XtPointer) 0},

  {"userLogin", "UserLogin", XtRString, sizeof(String),
   XtOffsetOf(AppData, userLogin), XtRImmediate, (XtPointer) 0},

  {"unixPW", "UnixPW", XtRString, sizeof(String),
   XtOffsetOf(AppData, unixPW), XtRImmediate, (XtPointer) 0},

  {"msLogon", "MSLogon", XtRString, sizeof(String),
   XtOffsetOf(AppData, msLogon), XtRImmediate, (XtPointer) 0},

  {"repeaterUltra", "RepeaterUltra", XtRString, sizeof(String),
   XtOffsetOf(AppData, repeaterUltra), XtRImmediate, (XtPointer) 0},

  {"ultraDSM", "UltraDSM", XtRBool, sizeof(Bool),
   XtOffsetOf(AppData, ultraDSM), XtRImmediate, (XtPointer) False},

  {"acceptPopup", "AcceptPopup", XtRBool, sizeof(Bool),
   XtOffsetOf(AppData, acceptPopup), XtRImmediate, (XtPointer) False},

  {"rfbVersion", "RfbVersion", XtRString, sizeof(String),
   XtOffsetOf(AppData, rfbVersion), XtRImmediate, (XtPointer) 0},

  {"passwordDialog", "PasswordDialog", XtRBool, sizeof(Bool),
   XtOffsetOf(AppData, passwordDialog), XtRImmediate, (XtPointer) False},

  {"encodings", "Encodings", XtRString, sizeof(String),
   XtOffsetOf(AppData, encodingsString), XtRImmediate, (XtPointer) 0},

  {"useBGR233", "UseBGR233", XtRInt, sizeof(int),
   XtOffsetOf(AppData, useBGR233), XtRImmediate, (XtPointer) 0},

  {"useBGR565", "UseBGR565", XtRBool, sizeof(Bool),
   XtOffsetOf(AppData, useBGR565), XtRImmediate, (XtPointer) False},

  {"useGreyScale", "UseGreyScale", XtRBool, sizeof(Bool),
   XtOffsetOf(AppData, useGreyScale), XtRImmediate, (XtPointer) False},

  {"yCrop", "yCrop", XtRInt, sizeof(int),
   XtOffsetOf(AppData, yCrop), XtRImmediate, (XtPointer) 0},

  {"sbWidth", "sbWidth", XtRInt, sizeof(int),
   XtOffsetOf(AppData, sbWidth), XtRImmediate, (XtPointer) 2},

  {"nColours", "NColours", XtRInt, sizeof(int),
   XtOffsetOf(AppData, nColours), XtRImmediate, (XtPointer) 256},

  {"useSharedColours", "UseSharedColours", XtRBool, sizeof(Bool),
   XtOffsetOf(AppData, useSharedColours), XtRImmediate, (XtPointer) True},

  {"forceOwnCmap", "ForceOwnCmap", XtRBool, sizeof(Bool),
   XtOffsetOf(AppData, forceOwnCmap), XtRImmediate, (XtPointer) False},

  {"forceTrueColour", "ForceTrueColour", XtRBool, sizeof(Bool),
   XtOffsetOf(AppData, forceTrueColour), XtRImmediate, (XtPointer) False},

  {"requestedDepth", "RequestedDepth", XtRInt, sizeof(int),
   XtOffsetOf(AppData, requestedDepth), XtRImmediate, (XtPointer) 0},

  {"useShm", "UseShm", XtRBool, sizeof(Bool),
   XtOffsetOf(AppData, useShm), XtRImmediate, (XtPointer) True},

  {"termChat", "TermChat", XtRBool, sizeof(Bool),
   XtOffsetOf(AppData, termChat), XtRImmediate, (XtPointer) False},

  {"wmDecorationWidth", "WmDecorationWidth", XtRInt, sizeof(int),
   XtOffsetOf(AppData, wmDecorationWidth), XtRImmediate, (XtPointer) 4},

  {"wmDecorationHeight", "WmDecorationHeight", XtRInt, sizeof(int),
   XtOffsetOf(AppData, wmDecorationHeight), XtRImmediate, (XtPointer) 24},

  {"popupButtonCount", "PopupButtonCount", XtRInt, sizeof(int),
   XtOffsetOf(AppData, popupButtonCount), XtRImmediate, (XtPointer) 0},

  {"popupButtonBreak", "PopupButtonBreak", XtRInt, sizeof(int),
   XtOffsetOf(AppData, popupButtonBreak), XtRImmediate, (XtPointer) 0},

  {"debug", "Debug", XtRBool, sizeof(Bool),
   XtOffsetOf(AppData, debug), XtRImmediate, (XtPointer) False},

  {"rawDelay", "RawDelay", XtRInt, sizeof(int),
   XtOffsetOf(AppData, rawDelay), XtRImmediate, (XtPointer) 0},

  {"copyRectDelay", "CopyRectDelay", XtRInt, sizeof(int),
   XtOffsetOf(AppData, copyRectDelay), XtRImmediate, (XtPointer) 0},

  {"bumpScrollTime", "BumpScrollTime", XtRInt, sizeof(int),
   XtOffsetOf(AppData, bumpScrollTime), XtRImmediate, (XtPointer) 25},

  {"bumpScrollPixels", "BumpScrollPixels", XtRInt, sizeof(int),
   XtOffsetOf(AppData, bumpScrollPixels), XtRImmediate, (XtPointer) 20},

  /* hardwired compress -1 vs . 7 */
  {"compressLevel", "CompressionLevel", XtRInt, sizeof(int),
   XtOffsetOf(AppData, compressLevel), XtRImmediate, (XtPointer) -1},

  /* hardwired quality was 6 */
  {"qualityLevel", "QualityLevel", XtRInt, sizeof(int),
   XtOffsetOf(AppData, qualityLevel), XtRImmediate, (XtPointer) -1},

  {"enableJPEG", "EnableJPEG", XtRBool, sizeof(Bool),
   XtOffsetOf(AppData, enableJPEG), XtRImmediate, (XtPointer) True},

  {"useRemoteCursor", "UseRemoteCursor", XtRBool, sizeof(Bool),
   XtOffsetOf(AppData, useRemoteCursor), XtRImmediate, (XtPointer) True},

  {"useCursorAlpha", "UseCursorAlpha", XtRBool, sizeof(Bool),
   XtOffsetOf(AppData, useCursorAlpha), XtRImmediate, (XtPointer) False},

  {"useRawLocal", "UseRawLocal", XtRBool, sizeof(Bool),
   XtOffsetOf(AppData, useRawLocal), XtRImmediate, (XtPointer) False},

  {"notty", "NoTty", XtRBool, sizeof(Bool),
   XtOffsetOf(AppData, notty), XtRImmediate, (XtPointer) False},

  {"useX11Cursor", "UseX11Cursor", XtRBool, sizeof(Bool),
   XtOffsetOf(AppData, useX11Cursor), XtRImmediate, (XtPointer) False},

  {"useBell", "UseBell", XtRBool, sizeof(Bool),
   XtOffsetOf(AppData, useBell), XtRImmediate, (XtPointer) True},

  {"grabKeyboard", "GrabKeyboard", XtRBool, sizeof(Bool),
   XtOffsetOf(AppData, grabKeyboard), XtRImmediate, (XtPointer) True},

  {"autoPass", "AutoPass", XtRBool, sizeof(Bool),
   XtOffsetOf(AppData, autoPass), XtRImmediate, (XtPointer) False},

  {"grabAll", "GrabAll", XtRBool, sizeof(Bool),
   XtOffsetOf(AppData, grabAll), XtRImmediate, (XtPointer) False},

  {"useXserverBackingStore", "UseXserverBackingStore", XtRBool, sizeof(Bool),
   XtOffsetOf(AppData, useXserverBackingStore), XtRImmediate, (XtPointer) False},

  {"overrideRedir", "OverrideRedir", XtRBool, sizeof(Bool),
   XtOffsetOf(AppData, overrideRedir), XtRImmediate, (XtPointer) True},

  {"serverInput", "ServerInput", XtRBool, sizeof(Bool),
   XtOffsetOf(AppData, serverInput), XtRImmediate, (XtPointer) True},

  {"singleWindow", "SingleWindow", XtRBool, sizeof(Bool),
   XtOffsetOf(AppData, singleWindow), XtRImmediate, (XtPointer) False},

  {"serverScale", "ServerScale", XtRInt, sizeof(int),
   XtOffsetOf(AppData, serverScale), XtRImmediate, (XtPointer) 1},

  {"chatActive", "ChatActive", XtRBool, sizeof(Bool),
   XtOffsetOf(AppData, chatActive), XtRImmediate, (XtPointer) False},

  {"chatOnly", "ChatOnly", XtRBool, sizeof(Bool),
   XtOffsetOf(AppData, chatOnly), XtRImmediate, (XtPointer) False},

  {"fileActive", "FileActive", XtRBool, sizeof(Bool),
   XtOffsetOf(AppData, fileActive), XtRImmediate, (XtPointer) False},

  {"popupFix", "PopupFix", XtRBool, sizeof(Bool),
   XtOffsetOf(AppData, popupFix), XtRImmediate, (XtPointer) False},

  {"scale", "Scale", XtRString, sizeof(String),
   XtOffsetOf(AppData, scale), XtRImmediate, (XtPointer) 0},

  {"pipelineUpdates", "PipelineUpdates", XtRBool, sizeof(Bool),
   XtOffsetOf(AppData, pipelineUpdates), XtRImmediate, (XtPointer)
#ifdef TURBOVNC
 True},
#else
#if 0
 False},
#else
 True},
#endif
#endif

  {"noipv4", "noipv4", XtRBool, sizeof(Bool),
   XtOffsetOf(AppData, noipv4), XtRImmediate, (XtPointer) False},

  {"noipv6", "noipv6", XtRBool, sizeof(Bool),
   XtOffsetOf(AppData, noipv6), XtRImmediate, (XtPointer) False},

  {"sendClipboard", "SendClipboard", XtRBool, sizeof(Bool),
   XtOffsetOf(AppData, sendClipboard), XtRImmediate, (XtPointer) False},

  {"sendAlways", "SendAlways", XtRBool, sizeof(Bool),
   XtOffsetOf(AppData, sendAlways), XtRImmediate, (XtPointer) False},

  {"recvText", "RecvText", XtRString, sizeof(String),
   XtOffsetOf(AppData, recvText), XtRImmediate, (XtPointer) 0},

  {"appShare", "AppShare", XtRBool, sizeof(Bool),
   XtOffsetOf(AppData, appShare), XtRImmediate, (XtPointer) False},

  {"escapeKeys", "EscapeKeys", XtRString, sizeof(String),
   XtOffsetOf(AppData, escapeKeys), XtRImmediate, (XtPointer) 0},

  {"escapeActive", "EscapeActive", XtRBool, sizeof(Bool),
   XtOffsetOf(AppData, escapeActive), XtRImmediate, (XtPointer) False}

  /* check commas */
};


/*
 * The cmdLineOptions array specifies how certain app resource specs can be set
 * with command-line options.
 */

XrmOptionDescRec cmdLineOptions[] = {
  {"-shared",        "*shareDesktop",       XrmoptionNoArg,  "True"},
  {"-noshared",      "*shareDesktop",       XrmoptionNoArg,  "False"},
  {"-viewonly",      "*viewOnly",           XrmoptionNoArg,  "True"},
  {"-fullscreen",    "*fullScreen",         XrmoptionNoArg,  "True"},
  {"-noraiseonbeep", "*raiseOnBeep",        XrmoptionNoArg,  "False"},
  {"-passwd",        "*passwordFile",       XrmoptionSepArg, 0},
  {"-user",          "*userLogin",          XrmoptionSepArg, 0},
  {"-unixpw",        "*unixPW",             XrmoptionSepArg, 0},
  {"-mslogon",       "*msLogon",            XrmoptionSepArg, 0},
  {"-repeater",      "*repeaterUltra",      XrmoptionSepArg, 0},
  {"-ultradsm",      "*ultraDSM",           XrmoptionNoArg,  "True"},
  {"-acceptpopup",   "*acceptPopup",        XrmoptionNoArg,  "True"},
  {"-acceptpopupsc", "*acceptPopup",        XrmoptionNoArg,  "True"},
  {"-rfbversion",    "*rfbVersion",         XrmoptionSepArg, 0},
  {"-encodings",     "*encodings",          XrmoptionSepArg, 0},
  {"-bgr233",        "*useBGR233",          XrmoptionNoArg,  "256"},
  {"-use64",         "*useBGR233",          XrmoptionNoArg,   "64"},
  {"-bgr222",        "*useBGR233",          XrmoptionNoArg,   "64"},
  {"-use8",          "*useBGR233",          XrmoptionNoArg,    "8"},
  {"-bgr111",        "*useBGR233",          XrmoptionNoArg,    "8"},
  {"-16bpp",         "*useBGR565",          XrmoptionNoArg,  "True"},
  {"-bgr565",        "*useBGR565",          XrmoptionNoArg,  "True"},
  {"-grey",          "*useGreyScale",       XrmoptionNoArg,  "True"},
  {"-gray",          "*useGreyScale",       XrmoptionNoArg,  "True"},
  {"-sbwidth",       "*sbwidth",            XrmoptionSepArg, 0},
  {"-env",           "*envDummy",           XrmoptionSepArg, 0},
  {"-ycrop",         "*yCrop",              XrmoptionSepArg, 0},
  {"-rawlocal",      "*useRawLocal",        XrmoptionNoArg,  "True"},
  {"-notty",         "*notty",              XrmoptionNoArg,  "True"},
  {"-alpha",         "*useCursorAlpha",     XrmoptionNoArg,  "True"},
  {"-owncmap",       "*forceOwnCmap",       XrmoptionNoArg,  "True"},
  {"-truecolor",     "*forceTrueColour",    XrmoptionNoArg,  "True"},
  {"-truecolour",    "*forceTrueColour",    XrmoptionNoArg,  "True"},
  {"-depth",         "*requestedDepth",     XrmoptionSepArg, 0},
  {"-compresslevel", "*compressLevel",      XrmoptionSepArg, 0},
  {"-quality",       "*qualityLevel",       XrmoptionSepArg, 0},
  {"-nojpeg",        "*enableJPEG",         XrmoptionNoArg,  "False"},
  {"-nocursorshape", "*useRemoteCursor",    XrmoptionNoArg,  "False"},
  {"-x11cursor",     "*useX11Cursor",       XrmoptionNoArg,  "True"},
  {"-nobell",        "*useBell",            XrmoptionNoArg,  "False"},
  {"-autopass",      "*autoPass",           XrmoptionNoArg,  "True"},
  {"-graball",       "*grabAll",            XrmoptionNoArg,  "True"},
  {"-grabkbd",       "*grabKeyboard",       XrmoptionNoArg,  "True"},
  {"-nograbkbd",     "*grabKeyboard",       XrmoptionNoArg,  "False"},
  {"-grabkeyboard",  "*grabKeyboard",       XrmoptionNoArg,  "True"},
  {"-nograbkeyboard","*grabKeyboard",       XrmoptionNoArg,  "False"},
  {"-nooverride",    "*overrideRedir",      XrmoptionNoArg,  "False"},
  {"-bs",            "*useXserverBackingStore",    XrmoptionNoArg,  "True"},
  {"-nobs",          "*useXserverBackingStore",    XrmoptionNoArg,  "False"},
  {"-popupfix",      "*popupFix",           XrmoptionNoArg,  "True"},
  {"-noshm",         "*useShm",             XrmoptionNoArg,  "False"},
  {"-termchat",      "*termChat",           XrmoptionNoArg,  "True"},
  {"-chatonly",      "*chatOnly",           XrmoptionNoArg,  "True"},
  {"-scale",         "*scale",              XrmoptionSepArg, 0},
  {"-appshare",      "*appShare",           XrmoptionNoArg,  "True"},
  {"-escape",        "*escapeKeys",         XrmoptionSepArg, 0},
  {"-sendclipboard", "*sendClipboard",      XrmoptionNoArg,  "True"},
  {"-sendalways",    "*sendAlways",         XrmoptionNoArg,  "True"},
  {"-recvtext",      "*recvText",           XrmoptionSepArg, 0},
  {"-pipeline",      "*pipelineUpdates",    XrmoptionNoArg,  "True"},
  {"-nopipeline",    "*pipelineUpdates",    XrmoptionNoArg,  "False"},
  {"-noipv4",        "*noipv4",             XrmoptionNoArg,  "True"},
  {"-noipv6",        "*noipv6",             XrmoptionNoArg,  "True"}
};

int numCmdLineOptions = XtNumber(cmdLineOptions);


/*
 * actions[] specifies actions that can be used in widget resource specs.
 */

static XtActionsRec actions[] = {
    {"SendRFBEvent", SendRFBEvent},
    {"ShowPopup", ShowPopup},
    {"Noop", Noop},
    {"HidePopup", HidePopup},
    {"HideScaleN", HideScaleN},
    {"HideTurboVNC", HideTurboVNC},
    {"HideQuality", HideQuality},
    {"HideCompress", HideCompress},
    {"ToggleFullScreen", ToggleFullScreen},
    {"JumpLeft", JumpLeft},
    {"JumpRight", JumpRight},
    {"JumpUp", JumpUp},
    {"JumpDown", JumpDown},
    {"SetFullScreenState", SetFullScreenState},
    {"SelectionFromVNC", SelectionFromVNC},
    {"SelectionToVNC", SelectionToVNC},
    {"ServerDialogDone", ServerDialogDone},
    {"UserDialogDone", UserDialogDone},
    {"YCropDialogDone", YCropDialogDone},
    {"ScbarDialogDone", ScbarDialogDone},
    {"ScaleNDialogDone", ScaleNDialogDone},
    {"ScaleDialogDone", ScaleDialogDone},
    {"PasswordDialogDone", PasswordDialogDone},
    {"Pause", Pause},
    {"RunCommand", RunCommand},
    {"Quit", Quit},
    {"HideChat", HideChat},
    {"Toggle8bpp", Toggle8bpp},
    {"Toggle16bpp", Toggle16bpp},
    {"ToggleFullColor", ToggleFullColor},
    {"Toggle256Colors", Toggle256Colors},
    {"Toggle64Colors", Toggle64Colors},
    {"Toggle8Colors", Toggle8Colors},
    {"ToggleGreyScale", ToggleGreyScale},
    {"ToggleTightZRLE", ToggleTightZRLE},
    {"ToggleTightHextile", ToggleTightHextile},
    {"ToggleZRLEZYWRLE", ToggleZRLEZYWRLE},
    {"ToggleViewOnly", ToggleViewOnly},
    {"ToggleJPEG", ToggleJPEG},
    {"ToggleCursorShape", ToggleCursorShape},
    {"ToggleCursorAlpha", ToggleCursorAlpha},
    {"ToggleX11Cursor", ToggleX11Cursor},
    {"ToggleBell", ToggleBell},
    {"ToggleRawLocal", ToggleRawLocal},
    {"ToggleServerInput", ToggleServerInput},
    {"TogglePipelineUpdates", TogglePipelineUpdates},
    {"ToggleSendClipboard", ToggleSendClipboard},
    {"ToggleSendAlways", ToggleSendAlways},
    {"ToggleSingleWindow", ToggleSingleWindow},
    {"ToggleTextChat", ToggleTextChat},
    {"ToggleFileXfer", ToggleFileXfer},
    {"ToggleXGrab", ToggleXGrab},
    {"DoServerScale", DoServerScale},
    {"SetScale", SetScale},
    {"SetYCrop", SetYCrop},
    {"SetScbar", SetScbar},
    {"ShowScaleN", ShowScaleN},
    {"ShowTurboVNC", ShowTurboVNC},
    {"ShowQuality", ShowQuality},
    {"ShowCompress", ShowCompress},
    {"SetScaleN", SetScaleN},
    {"SetTurboVNC", SetTurboVNC},
    {"SetQuality", SetQuality},
    {"SetCompress", SetCompress},
    {"Set8bppState", Set8bppState},
    {"Set16bppState", Set16bppState},
    {"SetFullColorState", SetFullColorState},
    {"Set256ColorsState", Set256ColorsState},
    {"Set64ColorsState", Set64ColorsState},
    {"Set8ColorsState", Set8ColorsState},
    {"SetGreyScaleState", SetGreyScaleState},
    {"SetZRLEState", SetZRLEState},
    {"SetHextileState", SetHextileState},
    {"SetZYWRLEState", SetZYWRLEState},
    {"SetNOJPEGState", SetNOJPEGState},
    {"SetScaleNState", SetScaleNState},
    {"SetQualityState", SetQualityState},
    {"SetCompressState", SetCompressState},
    {"SetViewOnlyState", SetViewOnlyState},
    {"SetCursorShapeState", SetCursorShapeState},
    {"SetCursorAlphaState", SetCursorAlphaState},
    {"SetX11CursorState", SetX11CursorState},
    {"SetBellState", SetBellState},
    {"SetRawLocalState", SetRawLocalState},
    {"SetServerInputState", SetServerInputState},
    {"SetPipelineUpdates", SetPipelineUpdates},
    {"SetSendClipboard", SetSendClipboard},
    {"SetSendAlways", SetSendAlways},
    {"SetSingleWindowState", SetSingleWindowState},
    {"SetTextChatState", SetTextChatState},
    {"SetFileXferState", SetFileXferState},
    {"SetXGrabState", SetXGrabState},
    {"SetEscapeKeysState", SetEscapeKeysState},
    {"ToggleEscapeActive", ToggleEscapeActive},
    {"EscapeDialogDone", EscapeDialogDone},
    {"SetEscapeKeys", SetEscapeKeys}
};


/*
 * removeArgs() is used to remove some of command line arguments.
 */

void
removeArgs(int *argc, char** argv, int idx, int nargs)
{
  int i;
  if ((idx+nargs) > *argc) return;
  for (i = idx+nargs; i < *argc; i++) {
    argv[i-nargs] = argv[i];
  }
  *argc -= nargs;
}

/*
 * usage() prints out the usage message.
 */

void
usage(void)
{
  fprintf(stdout,
	  "SSVNC Viewer (based on TightVNC viewer version 1.3.9)\n"
	  "\n"
	  "Usage: %s [<OPTIONS>] [<HOST>][:<DISPLAY#>]\n"
	  "       %s [<OPTIONS>] [<HOST>][::<PORT#>]\n"
	  "       %s [<OPTIONS>] exec=[CMD ARGS...]\n"
	  "       %s [<OPTIONS>] fd=n\n"
	  "       %s [<OPTIONS>] /path/to/unix/socket\n"
	  "       %s [<OPTIONS>] unix=/path/to/unix/socket\n"
	  "       %s [<OPTIONS>] -listen [<DISPLAY#>]\n"
	  "       %s -help\n"
	  "\n"
	  "<OPTIONS> are standard Xt options, or:\n"
	  "        -via <GATEWAY>\n"
	  "        -shared (set by default)\n"
	  "        -noshared\n"
	  "        -viewonly\n"
	  "        -fullscreen\n"
	  "        -noraiseonbeep\n"
	  "        -passwd <PASSWD-FILENAME> (standard VNC authentication)\n"
	  "        -user <USERNAME> (Unix login authentication)\n"
	  "        -encodings <ENCODING-LIST> (e.g. \"tight,copyrect\")\n"
	  "        -bgr233\n"
	  "        -owncmap\n"
	  "        -truecolour\n"
	  "        -depth <DEPTH>\n"
	  "        -compresslevel <COMPRESS-VALUE> (0..9: 0-fast, 9-best)\n"
	  "        -quality <JPEG-QUALITY-VALUE> (0..9: 0-low, 9-high)\n"
	  "        -nojpeg\n"
	  "        -nocursorshape\n"
	  "        -x11cursor\n"
	  "        -autopass\n"
	  "\n"
	  "Option names may be abbreviated, e.g. -bgr instead of -bgr233.\n"
	  "See the manual page for more information.\n"
	  "\n"
	  "\n"
	  "Enhanced TightVNC viewer (SSVNC) options:\n"
	  "\n"
	  "   URL http://www.karlrunge.com/x11vnc/ssvnc.html\n"
	  "\n"
	  "   Note: ZRLE and ZYWRLE encodings are now supported.\n"
	  "\n"
	  "   Note: F9 is shortcut to Toggle FullScreen mode.\n"
	  "\n"
	  "   Note: In -listen mode set the env var. SSVNC_MULTIPLE_LISTEN=1\n"
	  "         to allow more than one incoming VNC server at a time.\n"
	  "         This is the same as -multilisten described below.  Set\n"
	  "         SSVNC_MULTIPLE_LISTEN=MAX:n to allow no more than \"n\"\n"
	  "         simultaneous reverse connections.\n"
	  "\n"
	  "   Note: If the host:port is specified as \"exec=command args...\"\n"
	  "         then instead of making a TCP/IP socket connection to the\n"
	  "         remote VNC server, \"command args...\" is executed and the\n"
	  "         viewer is attached to its stdio.  This enables tunnelling\n"
	  "         established via an external command, e.g. an stunnel(8)\n"
	  "         that does not involve a listening socket.  This mode does\n"
	  "         not work for -listen reverse connections.  To not have the\n"
	  "         exec= pid killed at exit, set SSVNC_NO_KILL_EXEC_CMD=1.\n"
	  "\n"
	  "         If the host:port is specified as \"fd=n\" then it is assumed\n"
	  "         n is an already opened file descriptor to the socket. (i.e\n"
	  "         the parent did fork+exec)\n"
	  "\n"
	  "         If the host:port contains a '/' and exists in the file system\n"
	  "         it is interpreted as a unix-domain socket (AF_LOCAL/AF_UNIX\n"
	  "         instead of AF_INET)  Prefix with unix= to force interpretation\n"
	  "         as a unix-domain socket.\n"
	  "\n"
	  "        -multilisten  As in -listen (reverse connection listening) except\n"
	  "                    allow more than one incoming VNC server to be connected\n"
	  "                    at a time.  The default for -listen of only one at a\n"
	  "                    time tries to play it safe by not allowing anyone on\n"
	  "                    the network to put (many) desktops on your screen over\n"
	  "                    a long window of time. Use -multilisten for no limit.\n"
	  "\n"
	  "        -acceptpopup  In -listen (reverse connection listening) mode when\n"
	  "                    a reverse VNC connection comes in show a popup asking\n"
	  "                    whether to Accept or Reject the connection.  The IP\n"
	  "                    address of the connecting host is shown.  Same as\n"
	  "                    setting the env. var. SSVNC_ACCEPT_POPUP=1.\n"
	  "\n"
	  "        -acceptpopupsc  As in -acceptpopup except assume UltraVNC Single\n"
	  "                    Click (SC) server.  Retrieve User and ComputerName\n"
	  "                    info from UltraVNC Server and display in the Popup.\n"
	  "\n"
	  "        -use64      In -bgr233 mode, use 64 colors instead of 256.\n"
	  "        -bgr222     Same as -use64.\n"
	  "\n"
	  "        -use8       In -bgr233 mode, use 8 colors instead of 256.\n"
	  "        -bgr111     Same as -use8.\n"
	  "\n"
	  "        -16bpp      If the vnc viewer X display is depth 24 at 32bpp\n"
	  "                    request a 16bpp format from the VNC server to cut\n"
	  "                    network traffic by up to 2X, then tranlate the\n"
	  "                    pixels to 32bpp locally.\n"
	  "        -bgr565     Same as -16bpp.\n"
	  "\n"
	  "        -grey       Use a grey scale for the 16- and 8-bpp modes.\n"
	  "\n"
	  "        -alpha      Use alphablending transparency for local cursors\n"
	  "                    requires: x11vnc server, both client and server\n"
          "                    must be 32bpp and same endianness.\n"
	  "\n"
	  "        -scale str  Scale the desktop locally.  The string \"str\" can\n"
	  "                    a floating point ratio, e.g. \"0.9\", or a fraction,\n"
	  "                    e.g. \"3/4\", or WxH, e.g. 1280x1024.  Use \"fit\"\n"
	  "                    to fit in the current screen size.  Use \"auto\" to\n"
	  "                    fit in the window size.  \"str\" can also be set by\n"
	  "                    the env. var. SSVNC_SCALE.\n"
	  "\n"
	  "                    If you observe mouse trail painting errors, enable\n"
	  "                    X11 Cursor mode (either via Popup or -x11cursor.)\n"
	  "\n"
	  "                    Note that scaling is done in software and so can be\n"
	  "                    slow and requires more memory.  Some speedup Tips:\n"
	  "\n"
	  "                        ZRLE is faster than Tight in this mode.  When\n"
	  "                        scaling is first detected, the encoding will\n"
	  "                        be automatically switched to ZRLE.  Use the\n"
	  "                        Popup menu if you want to go back to Tight.\n"
	  "                        Set SSVNC_PRESERVE_ENCODING=1 to disable this.\n"
	  "\n"
	  "                        Use a solid background on the remote side.\n"
	  "                        (e.g. manually or via x11vnc -solid ...)\n"
	  "\n"
	  "                        If the remote server is x11vnc, try client\n"
	  "                        side caching: x11vnc -ncache 10 ...\n"
	  "\n"
	  "        -ycrop n    Only show the top n rows of the framebuffer.  For\n"
	  "                    use with x11vnc -ncache client caching option\n"
	  "                    to help \"hide\" the pixel cache region.\n"
	  "                    Use a negative value (e.g. -1) for autodetection.\n"
	  "                    Autodetection will always take place if the remote\n"
	  "                    fb height is more than 2 times the width.\n"
	  "\n"
	  "        -sbwidth n  Scrollbar width for x11vnc -ncache mode (-ycrop),\n"
          "                    default is very narrow: 2 pixels, it is narrow to\n"
          "                    avoid distraction in -ycrop mode.\n"
	  "\n"
	  "        -nobell     Disable bell.\n"
	  "\n"
	  "        -rawlocal   Prefer raw encoding for localhost, default is\n"
	  "                    no, i.e. assumes you have a SSH tunnel instead.\n"
	  "\n"
	  "        -notty      Try to avoid using the terminal for interactive\n"
	  "                    responses: use windows for messages and prompting\n"
	  "                    instead.  Messages will also be printed to terminal.\n"
	  "\n"
	  "        -sendclipboard  Send the X CLIPBOARD selection (i.e. Ctrl+C,\n"
	  "                        Ctrl+V) instead of the X PRIMARY selection (mouse\n"
	  "                        select and middle button paste.)\n"
	  "\n"
	  "        -sendalways     Whenever the mouse enters the VNC viewer main\n"
	  "                        window, send the selection to the VNC server even if\n"
	  "                        it has not changed.  This is like the Xt resource\n"
	  "                        translation SelectionToVNC(always)\n"
	  "\n"
	  "        -recvtext str   When cut text is received from the VNC server,\n"
	  "                        ssvncviewer will set both the X PRIMARY and the\n"
	  "                        X CLIPBOARD local selections.  To control which\n"
	  "                        is set, specify 'str' as 'primary', 'clipboard',\n"
	  "                        or 'both' (the default.)\n"
	  "\n"
	  "        -graball    Grab the entire X server when in fullscreen mode,\n"
	  "                    needed by some old window managers like fvwm2.\n"
	  "\n"
	  "        -popupfix   Warp the popup back to the pointer position,\n"
	  "                    needed by some old window managers like fvwm2.\n"
	  "        -sendclipboard  Send the X CLIPBOARD selection (i.e. Ctrl+C,\n"
	  "                        Ctrl+V) instead of the X PRIMARY selection (mouse\n"
	  "                        select and middle button paste.)\n"
	  "\n"
	  "        -sendalways     Whenever the mouse enters the VNC viewer main\n"
	  "                        window, send the selection to the VNC server even if\n"
	  "                        it has not changed.  This is like the Xt resource\n"
	  "                        translation SelectionToVNC(always)\n"
	  "\n"
	  "        -recvtext str   When cut text is received from the VNC server,\n"
	  "                        ssvncviewer will set both the X PRIMARY and the\n"
	  "                        X CLIPBOARD local selections.  To control which\n"
	  "                        is set, specify 'str' as 'primary', 'clipboard',\n"
	  "                        or 'both' (the default.)\n"
	  "\n"
	  "        -graball    Grab the entire X server when in fullscreen mode,\n"
	  "                    needed by some old window managers like fvwm2.\n"
	  "\n"
	  "        -popupfix   Warp the popup back to the pointer position,\n"
	  "                    needed by some old window managers like fvwm2.\n"
	  "\n"
	  "        -grabkbd    Grab the X keyboard when in fullscreen mode,\n"
	  "                    needed by some window managers. Same as -grabkeyboard.\n"
	  "                    -grabkbd is the default, use -nograbkbd to disable.\n"
	  "\n"
	  "        -bs, -nobs  Whether or not to use X server Backingstore for the\n"
	  "                    main viewer window.  The default is to not, mainly\n"
	  "                    because most Linux, etc, systems X servers disable\n"
	  "                    *all* Backingstore by default.  To re-enable it put\n"
	  "\n"
	  "                        Option \"Backingstore\"\n"
	  "\n"
	  "                    in the Device section of /etc/X11/xorg.conf.\n"
	  "                    In -bs mode with no X server backingstore, whenever an\n"
	  "                    area of the screen is re-exposed it must go out to the\n"
	  "                    VNC server to retrieve the pixels. This is too slow.\n"
	  "\n"
	  "                    In -nobs mode, memory is allocated by the viewer to\n"
	  "                    provide its own backing of the main viewer window. This\n"
	  "                    actually makes some activities faster (changes in large\n"
	  "                    regions) but can appear to \"flash\" too much.\n"
	  "\n"
	  "        -noshm      Disable use of MIT shared memory extension (not recommended)\n"
	  "\n"
	  "        -termchat   Do the UltraVNC chat in the terminal vncviewer is in\n"
	  "                    instead of in an independent window.\n"
	  "\n"
	  "        -unixpw str Useful for logging into x11vnc in -unixpw mode. \"str\" is a\n"
	  "                    string that allows many ways to enter the Unix Username\n"
	  "                    and Unix Password.  These characters: username, newline,\n"
	  "                    password, newline are sent to the VNC server after any VNC\n"
	  "                    authentication has taken place.  Under x11vnc they are\n"
	  "                    used for the -unixpw login.  Other VNC servers could do\n"
	  "                    something similar.\n"
	  "\n"
	  "                    You can also indicate \"str\" via the environment\n"
	  "                    variable SSVNC_UNIXPW.\n"
	  "\n"
	  "                    Note that the Escape key is actually sent first to tell\n"
	  "                    x11vnc to not echo the Unix Username back to the VNC\n"
	  "                    viewer. Set SSVNC_UNIXPW_NOESC=1 to override this.\n"
	  "\n"
	  "                    If str is \".\", then you are prompted at the command line\n"
	  "                    for the username and password in the normal way.  If str is\n"
	  "                    \"-\" the stdin is read via getpass(3) for username@password.\n"
	  "                    Otherwise if str is a file, it is opened and the first line\n"
	  "                    read is taken as the Unix username and the 2nd as the\n"
	  "                    password. If str prefixed by \"rm:\" the file is removed\n"
	  "                    after reading. Otherwise, if str has a \"@\" character,\n"
	  "                    it is taken as username@password. Otherwise, the program\n"
	  "                    exits with an error. Got all that?\n"
	  "\n"
	  "     -repeater str  This is for use with UltraVNC repeater proxy described\n"  
	  "                    here: http://www.uvnc.com/addons/repeater.html.  The \"str\"\n"  
	  "                    is the ID string to be sent to the repeater.  E.g. ID:1234\n"  
	  "                    It can also be the hostname and port or display of the VNC\n"  
	  "                    server, e.g. 12.34.56.78:0 or snoopy.com:1.  Note that when\n"  
	  "                    using -repeater, the host:dpy on the cmdline is the repeater\n"  
	  "                    server, NOT the VNC server.  The repeater will connect you.\n"  
	  "\n"
	  "                    Example: vncviewer ... -repeater ID:3333 repeat.host:5900\n"  
	  "                    Example: vncviewer ... -repeater vhost:0 repeat.host:5900\n"  
	  "\n"
	  "                    Use, e.g., '-repeater SCIII=ID:3210' if the repeater is a\n"
	  "                    Single Click III (SSL) repeater (repeater_SSL.exe) and you\n"
	  "                    are passing the SSL part of the connection through stunnel,\n"
	  "                    socat, etc. This way the magic UltraVNC string 'testB'\n"
          "                    needed to work with the repeater is sent to it.\n"
	  "\n"
	  "     -rfbversion str Set the advertised RFB version.  E.g.: -rfbversion 3.6\n"
	  "                    For some servers, e.g. UltraVNC this needs to be done.\n"
	  "\n"
	  "     -ultradsm      UltraVNC has symmetric private key encryption DSM plugins:\n"  
	  "                    http://www.uvnc.com/features/encryption.html. It is assumed\n"  
	  "                    you are using a unix program (e.g. our ultravnc_dsm_helper)\n"  
	  "                    to encrypt and decrypt the UltraVNC DSM stream. IN ADDITION\n"  
	  "                    TO THAT supply -ultradsm to tell THIS viewer to modify the\n"  
	  "                    RFB data sent so as to work with the UltraVNC Server. For\n"  
	  "                    some reason, each RFB msg type must be sent twice under DSM.\n"  
	  "\n"
	  "     -mslogon user  Use Windows MS Logon to an UltraVNC server.  Supply the\n"  
	  "                    username or \"1\" to be prompted.  The default is to\n"
	  "                    autodetect the UltraVNC MS Logon server and prompt for\n"
	  "                    the username and password.\n"
	  "\n"
	  "                    IMPORTANT NOTE: The UltraVNC MS-Logon Diffie-Hellman\n"
	  "                    exchange is very weak and can be brute forced to recover\n"
	  "                    your username and password in a few seconds of CPU time.\n"
	  "                    To be safe, be sure to use an additional encrypted tunnel\n"
	  "                    (e.g. SSL or SSH) for the entire VNC session.\n"
	  "\n"
	  "     -chatonly      Try to be a client that only does UltraVNC text chat. This\n"  
	  "                    mode is used by x11vnc to present a chat window on the\n"
	  "                    physical X11 console (i.e. chat with the person at the\n"
	  "                    display).\n"
	  "\n"
	  "     -env VAR=VALUE To save writing a shell script to set environment variables,\n"
	  "                    specify as many as you need on the command line.  For\n"
	  "                    example, -env SSVNC_MULTIPLE_LISTEN=MAX:5 -env EDITOR=vi\n"
	  "\n"
	  "     -noipv6        Disable all IPv6 sockets.  Same as VNCVIEWER_NO_IPV6=1.\n"
	  "\n"
	  "     -noipv4        Disable all IPv4 sockets.  Same as VNCVIEWER_NO_IPV4=1.\n"
	  "\n"
	  "     -printres      Print out the Ssvnc X resources (appdefaults) and then exit\n"  
	  "                    You can save them to a file and customize them (e.g. the\n"  
	  "                    keybindings and Popup menu)  Then point to the file via\n"  
	  "                    XENVIRONMENT or XAPPLRESDIR.\n"  
	  "\n"
	  "     -pipeline      Like TurboVNC, request the next framebuffer update as soon\n"
	  "                    as possible instead of waiting until the end of the current\n"
	  "                    framebuffer update coming in.  Helps 'pipeline' the updates.\n"
	  "                    This is currently the default, use -nopipeline to disable.\n"
	  "\n"
	  "     -appshare      Enable features for use with x11vnc's -appshare mode where\n"
	  "                    instead of sharing the full desktop only the application's\n"
	  "                    windows are shared.  Viewer multilisten mode is used to\n"
	  "                    create the multiple windows: -multilisten is implied.\n"
	  "                    See 'x11vnc -appshare -help' more information on the mode.\n"
	  "\n"
	  "                    Features enabled in the viewer under -appshare are:\n"
	  "                    Minimum extra text in the title, auto -ycrop is disabled,\n"
	  "                    x11vnc -remote_prefix X11VNC_APPSHARE_CMD: message channel,\n"
	  "                    x11vnc initial window position hints.  See also Escape Keys\n"
	  "                    below for additional key and mouse bindings.\n"
	  "\n"
	  "     -escape str    This sets the 'Escape Keys' modifier sequence and enables\n"
	  "                    escape keys mode.  When the modifier keys escape sequence\n"
	  "                    is held down, the next keystroke is interpreted locally\n"
	  "                    to perform a special action instead of being sent to the\n"
	  "                    remote VNC server.\n"
	  "\n"
	  "                    Use '-escape default' for the default modifier sequence.\n"
	  "                    (Unix: Alt_L,Super_L and MacOSX: Control_L,Meta_L)\n"
	  "\n"
	  "    Here are the 'Escape Keys: Help+Set' instructions from the Popup Menu:\n"
	  "\n"
          "    Escape Keys:  Enter a comma separated list of modifier keys to be the\n"
          "    'escape sequence'.  When these keys are held down, the next keystroke is\n"
          "    interpreted locally to invoke a special action instead of being sent to\n"
          "    the remote VNC server.  In other words, a set of 'Hot Keys'.\n"
          "    \n"
          "    To enable or disable this, click on 'Escape Keys: Toggle' in the Popup.\n"
          "    \n"
          "    Here is the list of hot-key mappings to special actions:\n"
          "    \n"
          "       r: refresh desktop  b: toggle bell   c: toggle full-color\n"
          "       f: file transfer    x: x11cursor     z: toggle Tight/ZRLE\n"
          "       l: full screen      g: graball       e: escape keys dialog\n"
          "       s: scale dialog     +: scale up (=)  -: scale down (_)\n"
          "       t: text chat                         a: alphablend cursor\n"
          "       V: toggle viewonly  Q: quit viewer   1 2 3 4 5 6: UltraVNC scale 1/n\n"
          "    \n"
          "       Arrow keys:         pan the viewport about 10%% for each keypress.\n"
          "       PageUp / PageDown:  pan the viewport by a screenful vertically.\n"
          "       Home   / End:       pan the viewport by a screenful horizontally.\n"
          "       KeyPad Arrow keys:  pan the viewport by 1 pixel for each keypress.\n"
          "       Dragging the Mouse with Button1 pressed also pans the viewport.\n"
          "       Clicking Mouse Button3 brings up the Popup Menu.\n"
          "    \n"
          "    The above mappings are *always* active in ViewOnly mode, unless you set the\n"
          "    Escape Keys value to 'never'.\n"
          "    \n"
          "    If the Escape Keys value below is set to 'default' then a default list of\n"
          "    of modifier keys is used.  For Unix it is: Alt_L,Super_L and for MacOSX it\n"
          "    is Control_L,Meta_L.  Note: the Super_L key usually has a Windows(TM) Flag\n"
          "    on it.  Also note the _L and _R mean the key is on the LEFT or RIGHT side\n"
          "    of the keyboard.\n"
          "    \n"
          "    On Unix   the default is Alt and Windows keys on Left side of keyboard.\n"
          "    On MacOSX the default is Control and Command keys on Left side of keyboard.\n"
          "    \n"
          "    Example: Press and hold the Alt and Windows keys on the LEFT side of the\n"
          "    keyboard and then press 'c' to toggle the full-color state.  Or press 't'\n"
          "    to toggle the ultravnc Text Chat window, etc.\n"
          "    \n"
          "    To use something besides the default, supply a comma separated list (or a\n"
          "    single one) from: Shift_L Shift_R Control_L Control_R Alt_L Alt_R Meta_L\n"
          "    Meta_R Super_L Super_R Hyper_L Hyper_R or Mode_switch.\n"
	  "\n"
	  "\n"
	  "   New Popup actions:\n"
	  "\n"
	  "        ViewOnly:                ~ -viewonly\n"
	  "        Disable Bell:            ~ -nobell\n"
	  "        Cursor Shape:            ~ -nocursorshape\n"
	  "        X11 Cursor:              ~ -x11cursor\n"
	  "        Cursor Alphablend:       ~ -alpha\n"
	  "        Toggle Tight/Hextile:    ~ -encodings hextile...\n"
	  "        Toggle Tight/ZRLE:       ~ -encodings zrle...\n"
	  "        Toggle ZRLE/ZYWRLE:      ~ -encodings zywrle...\n"
	  "        Quality Level            ~ -quality (both Tight and ZYWRLE)\n"
	  "        Compress Level           ~ -compresslevel\n"
	  "        Disable JPEG:            ~ -nojpeg  (Tight)\n"
	  "        Pipeline Updates         ~ -pipeline\n"
	  "\n"
	  "        Full Color                 as many colors as local screen allows.\n"
	  "        Grey scale (16 & 8-bpp)  ~ -grey, for low colors 16/8bpp modes only.\n"
	  "        16 bit color (BGR565)    ~ -16bpp / -bgr565\n"
	  "        8  bit color (BGR233)    ~ -bgr233\n"
	  "        256 colors               ~ -bgr233 default # of colors.\n"
	  "         64 colors               ~ -bgr222 / -use64\n"
	  "          8 colors               ~ -bgr111 / -use8\n"
	  "        Scale Viewer             ~ -scale\n"
	  "        Escape Keys: Toggle      ~ -escape\n"
	  "        Escape Keys: Help+Set    ~ -escape\n"
	  "        Set Y Crop (y-max)       ~ -ycrop\n"
	  "        Set Scrollbar Width      ~ -sbwidth\n"
	  "        XGrabServer              ~ -graball\n"
	  "\n"
	  "        UltraVNC Extensions:\n"
	  "\n"
	  "          Set 1/n Server Scale     Ultravnc ext. Scale desktop by 1/n.\n"
	  "          Text Chat                Ultravnc ext. Do Text Chat.\n"
	  "          File Transfer            Ultravnc ext. File xfer via Java helper.\n"
	  "          Single Window            Ultravnc ext. Grab and view a single window.\n"
	  "                                   (select then click on the window you want).\n"
	  "          Disable Remote Input     Ultravnc ext. Try to prevent input and\n"
	  "                                   viewing of monitor at physical display.\n"
	  "\n"
	  "        Note: the Ultravnc extensions only apply to servers that support\n"
	  "              them.  x11vnc/libvncserver supports some of them.\n"
	  "\n"
	  "        Send Clipboard not Primary  ~ -sendclipboard\n"
	  "        Send Selection Every time   ~ -sendalways\n"
	  "\n"
	  "\n", programName, programName, programName, programName, programName, programName, programName, programName);
  exit(1);
}
#if 0
	  "        -nooverride Do not apply OverrideRedirect in fullscreen mode.\n"
#endif


/*
 * GetArgsAndResources() deals with resources and any command-line arguments
 * not already processed by XtVaAppInitialize().  It sets vncServerHost and
 * vncServerPort and all the fields in appData.
 */
extern int saw_appshare;

void
GetArgsAndResources(int argc, char **argv)
{
	char *vncServerName = NULL, *colonPos, *bracketPos;
	int len, portOffset;
	int disp;

  /* Turn app resource specs into our appData structure for the rest of the
     program to use */

	XtGetApplicationResources(toplevel, &appData, appDataResourceList,
	    XtNumber(appDataResourceList), 0, 0);

	/*
	 * we allow setting of some by env, to avoid clash with other
	 * viewer's cmdlines (e.g. change viewer in SSVNC).
	 */
	if (getenv("VNCVIEWER_ALPHABLEND")) {
		appData.useCursorAlpha = True;
	}
	if (getenv("VNCVIEWER_POPUP_FIX")) {
		if (getenv("NOPOPUPFIX")) {
			;
		} else if (!strcmp(getenv("VNCVIEWER_POPUP_FIX"), "0")) {
			;
		} else {
			appData.popupFix = True;
		}
	}
	if (getenv("VNCVIEWER_GRAB_SERVER")) {
		appData.grabAll = True;
	}
	if (getenv("VNCVIEWER_YCROP")) {
		int n = atoi(getenv("VNCVIEWER_YCROP"));
		if (n != 0) {
			appData.yCrop = n;
		}
	}
	if (getenv("VNCVIEWER_RFBVERSION") && strcmp(getenv("VNCVIEWER_RFBVERSION"), "")) {
		appData.rfbVersion = strdup(getenv("VNCVIEWER_RFBVERSION"));
	}
	if (getenv("VNCVIEWER_ENCODINGS") && strcmp(getenv("VNCVIEWER_ENCODINGS"), "")) {
		appData.encodingsString = strdup(getenv("VNCVIEWER_ENCODINGS"));
	}
	if (getenv("VNCVIEWER_NOBELL")) {
		appData.useBell = False;
	}
	if (getenv("VNCVIEWER_X11CURSOR")) {
		appData.useX11Cursor = True;
	}
	if (getenv("VNCVIEWER_RAWLOCAL")) {
		appData.useRawLocal = True;
	}
	if (getenv("VNCVIEWER_NOTTY") || getenv("SSVNC_VNCVIEWER_NOTTY")) {
		appData.notty = True;
	}
	if (getenv("VNCVIEWER_SBWIDTH")) {
		int n = atoi(getenv("VNCVIEWER_SBWIDTH"));
		if (n != 0) {
			appData.sbWidth = n;
		}
	}
	if (getenv("VNCVIEWER_ULTRADSM")) {
		appData.ultraDSM = True;
	}
	if (getenv("SSVNC_ULTRA_DSM") && strcmp(getenv("SSVNC_ULTRA_DSM"), "")) {
		appData.ultraDSM = True;
	}
	if (getenv("SSVNC_NO_ULTRA_DSM")) {
		appData.ultraDSM = False;
	}
	if (getenv("SSVNC_SCALE") && strcmp(getenv("SSVNC_SCALE"), "")) {
		if (appData.scale == NULL) {
			appData.scale = strdup(getenv("SSVNC_SCALE"));
		}
	}
	if (getenv("VNCVIEWER_ESCAPE") && strcmp(getenv("VNCVIEWER_ESCAPE"), "")) {
		if (appData.escapeKeys == NULL) {
			appData.escapeKeys = strdup(getenv("VNCVIEWER_ESCAPE"));
		}
	}
	if (saw_appshare) {
		appData.appShare = True;
	}
	if (appData.appShare && appData.escapeKeys == NULL) {
		appData.escapeKeys = strdup("default");
	}
	if (appData.escapeKeys != NULL) {
		appData.escapeActive = True;
	}
	if (getenv("VNCVIEWER_SEND_CLIPBOARD")) {
		appData.sendClipboard = True;
	}
	if (getenv("VNCVIEWER_SEND_ALWAYS")) {
		appData.sendAlways = True;
	}
	if (getenv("VNCVIEWER_RECV_TEXT")) {
		char *s = getenv("VNCVIEWER_RECV_TEXT");
		if (!strcasecmp(s, "clipboard")) {
			appData.recvText = strdup("clipboard");
		} else if (!strcasecmp(s, "primary")) {
			appData.recvText = strdup("primary");
		} else if (!strcasecmp(s, "both")) {
			appData.recvText = strdup("both");
		}
	}
	if (getenv("VNCVIEWER_PIPELINE_UPDATES")) {
		appData.pipelineUpdates = True;
	} else if (getenv("VNCVIEWER_NO_PIPELINE_UPDATES")) {
		appData.pipelineUpdates = False;
	}

	if (getenv("VNCVIEWER_NO_IPV4")) {
		appData.noipv4 = True;
	}
	if (getenv("VNCVIEWER_NO_IPV6")) {
		appData.noipv6 = True;
	}

	if (appData.useBGR233 && appData.useBGR565) {
		appData.useBGR233 = 0;
	}

	if (getenv("SSVNC_ULTRA_FTP_JAR") == NULL) {
	    putenv("SSVNC_ULTRA_FTP_JAR=/usr/lib/ssvnc/util/ultraftp.jar");
	}


  /* Add our actions to the actions table so they can be used in widget
     resource specs */

	XtAppAddActions(appContext, actions, XtNumber(actions));

  /* Check any remaining command-line arguments.  If -listen was specified
     there should be none.  Otherwise the only argument should be the VNC
     server name.  If not given then pop up a dialog box and wait for the
     server name to be entered. */

	if (listenSpecified) {
		if (argc != 1) {
			fprintf(stderr,"\n%s -listen: invalid command line argument: %s\n",
			programName, argv[1]);
			usage();
		}
		return;
	}

	if (argc == 1) {
		vncServerName = DoServerDialog();
		if (!use_tty()) {
			appData.passwordDialog = True;
		}
	} else if (argc != 2) {
		usage();
	} else {
		vncServerName = argv[1];

		if (!use_tty()) {
			appData.passwordDialog = True;
		}
		if (vncServerName[0] == '-') {
			usage();
		}
	}

	if (strlen(vncServerName) > 255) {
		fprintf(stderr,"VNC server name too long\n");
		exit(1);
	}

	colonPos = strrchr(vncServerName, ':');
	bracketPos = strrchr(vncServerName, ']');
	if (strstr(vncServerName, "exec=") == vncServerName) {
		/* special exec-external-command case */
		strcpy(vncServerHost, vncServerName);
		vncServerPort = SERVER_PORT_OFFSET;
	} else if (strstr(vncServerName, "fd=") == vncServerName) {
		/* special exec-external-command case */
		strcpy(vncServerHost, vncServerName);
		vncServerPort = SERVER_PORT_OFFSET;
	} else if (colonPos == NULL) {
		/* No colon -- use default port number */
		strcpy(vncServerHost, vncServerName);
		vncServerPort = SERVER_PORT_OFFSET;
	} else if (bracketPos != NULL && colonPos < bracketPos) {
		strcpy(vncServerHost, vncServerName);
		vncServerPort = SERVER_PORT_OFFSET;
	} else {
		if (colonPos > vncServerName && *(colonPos - 1) == ':') {
			colonPos--;
		}
		memcpy(vncServerHost, vncServerName, colonPos - vncServerName);
		vncServerHost[colonPos - vncServerName] = '\0';
		len = strlen(colonPos + 1);
		portOffset = SERVER_PORT_OFFSET;
		if (colonPos[1] == ':') {
			/* Two colons -- interpret as a port number */
			colonPos++;
			len--;
			portOffset = 0;
		}
		if (!len || strspn(colonPos + 1, "0123456789") != (size_t) len) {
			usage();
		}
#if 0
		vncServerPort = atoi(colonPos + 1) + portOffset;
#else
		disp = atoi(colonPos + 1);
		if (portOffset != 0 && disp >= 100) {
			portOffset = 0;
		}
		vncServerPort = disp + portOffset;
#endif
	}
}
