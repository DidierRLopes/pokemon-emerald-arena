-- Loads the voice bridge in the running mGBA: Tools > Scripting…, then the
-- scripting window's File menu > emerald-voice-bridge.lua (a recent script).
-- mGBA 0.10 has no command-line option for this. macOS asks once to allow
-- the calling app (Terminal) under Privacy & Security > Accessibility.
on run
	tell application "mGBA" to activate
	delay 0.6
	tell application "System Events"
		tell process "mGBA"
			set toolsMenu to menu "Tools" of menu bar 1
			click (first menu item of toolsMenu whose name starts with "Scripting")
			delay 1.2
			repeat with barItem in menu bar items of menu bar 1
				try
					set found to (first menu item of menu 1 of barItem whose name contains "emerald-voice-bridge")
					click found
					return "loaded"
				end try
			end repeat
		end tell
	end tell
	return "not found"
end run
