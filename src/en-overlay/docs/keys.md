# Keys and pad

Every key and pad binding, EldenNet's and every other mod's, is changed in one place: **GraceUI › Hotkeys**, in the
game's menu or in the browser view while the game runs. The keys below are the defaults EldenNet and GraceUI register.

## The defaults

| Key | Pad | What it does | When |
|---|---|---|---|
| F5 | None by default | Opens and closes GraceUI's menu | Whenever it is safe; GraceUI's own key |
| T | D-pad Up, then View | Talk: opens the chat box with the character you are near | Near a character it voices |
| Tab | None | Skip the page: shows the next page of a reply at once | While a character's reply shows |
| F11 | None | Opens and closes EldenNet's debug view | Only with Developer views on |
| Pause | None | Mark (development): marks a moment, for testers | Only in test runs, or with Developer views on |

For the pad, hold D-pad Up, then press View. The first press of D-pad Up still reaches the game.

## Talking to a character

1. Walk up to a character EldenNet voices. When the talk key would open a conversation, a hint shows it, with the
   character's name.
2. Press **T**, or hold D-pad Up and press View. The chat box opens, headed with whom you speak to and who hears you.
3. Type your line, up to 240 characters, and press **Enter**. The character's reply shows a page at a time.
4. Press **Esc** to close the box. A reply already under way still finishes showing.

| In the chat box | Key | Pad |
|---|---|---|
| Send your line | Enter | A |
| Close the box | Esc | B |
| Skip to the next page of a reply | Tab | None |

While the box is open, your keys and pad go to the box, not the game. The mouse is not used. Danger closes the box,
and ends the conversation if it lasts. A conversation also ends when you walk away, after two minutes without a line
from you, on a loading screen and when you die.

> **Note.** This version shows no on-screen keyboard. On a pad, the box opens, but you type with a keyboard, or with
> an on-screen keyboard your system or Steam offers.

## In GraceUI's menu

- **F5** again, or **Esc** or **B** at the top level, closes the menu. Esc and B close a pop-up first, then go back a
  level.
- The menu shows its keys for the part you are on along its foot.
- The menu opens only when it is safe. When danger comes near, every view closes at once.
- To get out of anything that holds your keys, press the menu key twice within 2 seconds, or hold **Esc** for 1
  second. Nothing can take this way out from you.
- There is no pad button for the menu by default, since every button has a use in the game. You can map a spare
  button, or a back button on a handheld, to F5 with Steam Input.

## Changing a key

On GraceUI › Hotkeys, each row offers Change key, Change pad chord and Reset, and Reset every hotkey puts all of them
back. If you choose a key that another hotkey has, GraceUI asks first, then moves it, leaving the other with none.

While a key is bound to one of EldenNet's hotkeys, the game never sees it: T and Tab do nothing in the game. If your
own layout uses one of them, give EldenNet's hotkey another key.

If another mod registered a key first, EldenNet's hotkey is left with no key, and GraceUI › Hotkeys says which mod
took it. Give it a key there.

## See also

[Getting started](doc:getting-started) · [Troubleshooting](doc:troubleshooting)
