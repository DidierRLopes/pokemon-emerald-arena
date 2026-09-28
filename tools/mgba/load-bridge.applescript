-- Loads the voice bridge in the running mGBA: Tools > Scripting..., then the
-- scripting window's File > Load recent script > emerald-voice-bridge.lua.
-- mGBA 0.10 has no command-line option for this. macOS asks once to allow
-- the calling app (Terminal) under Privacy & Security > Accessibility.
on run
	tell application "mGBA" to activate
	delay 0.6
	tell application "System Events"
		tell process "mGBA"
			-- The scripting window may already be open (its menu bar has no Tools).
			if exists menu "Tools" of menu bar 1 then
				click (first menu item of menu "Tools" of menu bar 1 whose name starts with "Scripting")
				delay 1.2
			end if
			try
				set recentMenu to menu 1 of (first menu item of menu "File" of menu bar 1 whose name starts with "Load recent")
				click (first menu item of recentMenu whose name contains "emerald-voice-bridge")
				return "loaded"
			end try
		end tell
	end tell
	return "not found"
end run
