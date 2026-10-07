# The Original Client's Settings Menu

*Written by `scripts/generate-stock-options-catalog` from the original 3.3.5a
client's own interface files (enUS/patch-enUS-3.MPQ and its
siblings). Do not edit by hand: re-run the script.*

Every setting the original client's Options menu offers, panel by panel,
the reference for the custom client's own menu (issue 173b and its
sub-issues). **Stored name** is the setting's name in the original (a
"CVar"; the same name a stock `WTF/Config.wtf` uses). **Kind** is a tick
box, a slider (with its range and step), or a drop-down (with its
choices and the value each stores). **Needs** marks the few the
original applies only after a restart or a logout.

## Video

### Resolution

| Setting | Kind | Stored name | Needs | What the original says |
|---|---|---|---|---|
| Use UI Scale | tick box | `useUiScale` |  | Check to use the UI Scale Slider, uncheck to use the system default scale. |
| Vertical Sync | tick box | `gxVSync` |  | Synchronizes your framerate to some fraction of your monitor's refresh rate. Enable this if you see excessive screen tearing in game. |
| Triple Buffering | tick box | `gxTripleBuffer` |  | Enables triple buffering of frames when vertical sync is enabled. Selecting triple buffering may even out frame rate spikes, but may also cause slight input lag. |
| Hardware Cursor | tick box | `gxCursor` |  | Enable this option for a more responsive cursor unless you have cursor problems. |
| Reduce Input Lag | tick box | `gxFixLag` |  | Enabling this reduces user interface lag, but may drastically reduce frame rates. |
| Windowed Mode | tick box | `gxWindow` |  | Check to play in a non-fullscreen window.  If this is checked the game will use your desktop gamma and you will not be able to adjust it via the slider below. |
| Maximized | tick box | `gxMaximize` |  | Check to maximize window and remove borders. |
| Disable Resize | tick box | `windowResizeLock` |  | Enable this to prevent resizing the game window. |
| Use desktop gamma | tick box | `desktopGamma` |  | Use the same gamma settings as your desktop. |
| Gamma | slider -0.5 to 0.5, step 0.1 | `gamma` |  | Controls the brightness of the game. Increase brightness until you can clearly see all 21 levels of gray bars below. |
| (no label; drawn by its panel) | slider 0.64 to 1, step 0.01 | `uiscale` |  |  |
| MultiSample | drop-down: computed by the original at run time (from the machine or the account) | `none: set by a program call, not a stored name` | | |
| Refresh | drop-down: computed by the original at run time (from the machine or the account) | `gxRefresh` | | |
| Resolution | drop-down: computed by the original at run time (from the machine or the account) | `none: set by a program call, not a stored name` | | |

### Effects

| Setting | Kind | Stored name | Needs | What the original says |
|---|---|---|---|---|
| View Distance | slider 177 to 1277, step 110 | `farclip` |  | Changes how far you can see. Decreasing this may greatly improve performance. |
| Terrain Blending | slider 0 to 1, step 1 | `TerrainMip` | logout |  |
| Particle Density | slider 0.1 to 1, step 0.1 | `particleDensity` |  | Controls the number of particles used in effects caused by spells, fires, etc. Decrease to improve performance. |
| Environment Detail | slider 0.5 to 1.5, step 0.25 | `environmentDetail` |  | Controls how far you can see objects. Decrease to improve performance. |
| Ground Clutter Density | slider 16 to 64, step 8 | `groundEffectDensity` |  | Controls the number of ground clutter items, like grass and foliage.  Decrease to improve performance. |
| Ground Clutter Radius | slider 70 to 140, step 10 | `groundEffectDist` |  | Controls the draw distance of ground clutter items like grass and foliage.  Decrease to improve performance. |
| Texture Resolution | slider 0 to 1, step 1 | `BaseMip` |  | Controls the level of all texture detail. Decreasing this may slightly improve performance. |
| Shadow Quality | slider 0 to 4, step 1 | `extShadowQuality` |  | Controls the quality of rendered character and environment shadows. Decrease to improve performance. |
| Texture Filtering | slider 0 to 5, step 1 | `textureFilteringMode` | restart | Increases texture sharpness, particularly for textures viewed at an angle.  Decrease to improve performance. |
| Weather Intensity | slider 0 to 3, step 1 | `weatherDensity` |  | Controls the intensity of weather effects.  Decrease to improve performance. |
| Player Textures | slider 8 to 9, step 1 | `componentTextureLevel` | restart | Controls the resolution of player textures. Decreasing this may slightly improve performance. |
| Specular Lighting | tick box | `specular` | logout | Enables specular highlights on terrain. Disabling this can sometimes improve performance. |
| Full-Screen Glow Effect | tick box | `ffxGlow` |  | Enables a full screen effect that softens edges and lighting. Disabling this can sometimes improve performance. |
| Death Effect | tick box | `ffxDeath` |  | Uncheck to disable glowing effect while in ghost form. Disabling this can sometimes improve performance. |
| Projected Textures | tick box | `projectedTextures` |  | Enables the projecting of textures to the environment. Disabling this may greatly improve performance. |
| (no label; drawn by its panel) | slider 1 to 6, step 1 | `quality` |  |  |

### Stereo

| Setting | Kind | Stored name | Needs | What the original says |
|---|---|---|---|---|
| Enable Stereo Video | tick box | `gxStereoEnabled` |  | Enables stereoscopic (3D) video. |
| Screen Depth | slider 0.2 to 50, step 0.1 | `gxStereoConvergence` |  |  |
| Depth Amount | slider 0 to 100, step 1 | `gxStereoSeparation` |  |  |
| Hardware Cursor | tick box | `gxCursor` |  | While enabling this option may improve performance, the 3D cursor functionality will not work with it enabled. |

## Sound & Voice

### Sound

| Setting | Kind | Stored name | Needs | What the original says |
|---|---|---|---|---|
| Error Speech | tick box | `Sound_EnableErrorSpeech` |  | Enables speech from errors (e.g. "Target out of range"). |
| Music | tick box | `Sound_EnableMusic` |  | Enable background music. |
| Ambient Sounds | tick box | `Sound_EnableAmbience` |  | Enable ambient sounds. |
| Sound Effects | tick box | `Sound_EnableSFX` |  | Enables or disables game sound effects. |
| Enable Sound | tick box | `Sound_EnableAllSound` |  | Enables or disables all sound. |
| Sound at Character | tick box | `Sound_ListenerAtCharacter` |  | Listen to sound from the character's point of view rather than the camera. |
| Emote Sounds | tick box | `Sound_EnableEmoteSounds` |  | Toggles emote sounds on and off. |
| Enable Pet Sounds | tick box | `Sound_EnablePetSounds` |  | Toggles pet idle and aggro sounds on and off. |
| Loop Music | tick box | `Sound_ZoneMusicNoDelay` |  | Enable to continuously play background music. |
| Sound in Background | tick box | `Sound_EnableSoundWhenGameIsInBG` |  | Enable to allow sounds to play even when World of Warcraft is in the background. |
| Enable Reverb | tick box | `Sound_EnableReverb` |  | Enables reverb sound effects.  This will affect performance. |
| Use Hardware | tick box | `Sound_EnableHardware` |  | Enables the use of hardware for 3D sound management. This may alter your sound performance. |
| Headphone mode | tick box | `Sound_EnableSoftwareHRTF` |  | Enables software emulation for headphone surround sound. |
| Death Knight Voices | tick box | `Sound_EnableDSPEffects` |  | Special voice modulation effects for Death Knights. Disable to improve performance. |
| Sound | slider 0 to 1, step 0.1 | `Sound_SFXVolume` |  | Adjusts the sound effect volume. |
| Music | slider 0 to 1, step 0.1 | `Sound_MusicVolume` |  | Adjusts the background music volume. |
| Ambience | slider 0 to 1, step 0.1 | `Sound_AmbienceVolume` |  | Adjusts the ambient sound volume. |
| Master Volume | slider 0 to 1, step SOUND_MASTERVOLUME_STEP | `Sound_MasterVolume` |  | Adjusts the master sound volume. |
| Sound Channels | slider 32 to 64, step 32 | `Sound_NumChannels` |  | Changes the number of active software sound channels. |
| Sound Quality | slider 0 to 2, step 1 | `Sound_OutputQuality` |  | Adjusts the quality of game sounds. Decrease to improve performance. |
| Hardware | drop-down: computed by the original at run time (from the machine or the account) | `Sound_OutputDriverIndex` | | |

### Voice

| Setting | Kind | Stored name | Needs | What the original says |
|---|---|---|---|---|
| Enable Voice Chat | tick box | `EnableVoiceChat` |  | Voice Chat is not monitored by Blizzard Entertainment, and your gameplay experience may change based on interaction with other players. |
| Enable Microphone | tick box | `EnableMicrophone` |  | Enables your microphone. |
| Microphone Volume | slider 0.25 to 2.5, step 0.05 | `OutboundChatVolume` |  | Adjusts the volume of your voice. |
| Speaker Volume | slider 0 to 1, step 0.01 | `InboundChatVolume` |  | Adjusts the volume of others' voices. |
| (no label; drawn by its panel) | slider 0 to 1, step 0.01 | `ChatSoundVolume` |  |  |
| (no label; drawn by its panel) | slider 0 to 1, step 0.01 | `ChatMusicVolume` |  |  |
| (no label; drawn by its panel) | slider 0 to 1, step 0.01 | `ChatAmbienceVolume` |  |  |
| Push-to-Talk Sound | tick box | `PushToTalkSound` |  |  |
| Activation Sensitivity | slider 0 to 1, step 0.02 | `VoiceActivationSensitivity` |  | Adjusts the sensitivity of the microphone. |
| ChatMode | drop-down: Push-to-Talk = `0`; Voice Activated = `1` | `VoiceChatMode` | | |
| InputDevice | drop-down: computed by the original at run time (from the machine or the account) | `Sound_VoiceChatInputDriverIndex` | | |
| OutputDevice | drop-down: computed by the original at run time (from the machine or the account) | `Sound_VoiceChatOutputDriverIndex` | | |

## Interface

### Controls

| Setting | Kind | Stored name | Needs | What the original says |
|---|---|---|---|---|
| Sticky Targeting | tick box | `deselectOnClick` |  |  |
| Auto Dismount in Flight | tick box | `autoDismountFlying` |  |  |
| Auto Clear Away | tick box | `autoClearAFK` |  | Automatically exit Away mode upon moving or talking. |
| Block Trades | tick box | `blockTrades` |  | Block all incoming trade requests. |
| Open Loot Window at Mouse | tick box | `lootUnderMouse` |  |  |
| Auto Loot | tick box | `autoLootDefault` |  |  |
| Auto Loot Key | tick box | `autoLootKey` |  | Key to use to auto loot |
| AutoLootKey | drop-down: ALT key = `ALT`; CTRL key = `CTRL`; SHIFT key = `SHIFT`; None = `NONE` | `none: set by a program call, not a stored name` | | |

### Combat

| Setting | Kind | Stored name | Needs | What the original says |
|---|---|---|---|---|
| Attack on Assist | tick box | `assistAttack` |  | Automatically attack a target when you acquire it by using "/assist". |
| Auto Attack/Auto Shot | tick box | `autoRangedCombat` |  |  |
| Auto Self Cast | tick box | `autoSelfCast` |  |  |
| Stop Auto Attack | tick box | `stopAutoAttackOnTargetChange` |  | Stop auto attacking when you switch targets. |
| Target of Target | tick box | `showTargetOfTarget` |  |  |
| On Targets | tick box | `showTargetCastbar` |  | Show the spell that your current target is casting. |
| On Nameplates | tick box | `showVKeyCastbar` |  | If the Name Plate is displayed for your current target, show the enemy cast bar with the target's health bar in the game field. |
| Class Colors in Nameplates | tick box | `ShowClassColorInNameplate` |  | Toggles class colors in the background of Nameplate health bar for enemy players. |
| FocusCastKey | drop-down: ALT key = `ALT`; CTRL key = `CTRL`; SHIFT key = `SHIFT`; None = `NONE` | `none: set by a program call, not a stored name` | | |
| SelfCastKey | drop-down: ALT key = `ALT`; CTRL key = `CTRL`; SHIFT key = `SHIFT`; None = `NONE` | `none: set by a program call, not a stored name` | | |
| TOT | drop-down: Raid = `1`; Party = `2`; Solo = `3`; Raid & Party = `4`; Always = `5` | `targetOfTargetMode` | | |

### Display

| Setting | Kind | Stored name | Needs | What the original says |
|---|---|---|---|---|
| Rotate Minimap | tick box | `rotateMinimap` |  | Check this to rotate the entire minimap instead of the player arrow. |
| Screen Edge Flash | tick box | `screenEdgeFlash` |  |  |
| Detailed Loot Information | tick box | `showLootSpam` |  | Uncheck this to hide individual loot roll messages and only show the winner. |
| Show Free Bag Space | tick box | `displayFreeBagSlots` |  | Displays remaining inventory space on the backpack icon. |
| Show Clock | tick box | `showClock` |  | Displays the clock button on the minimap. |
| Cinematic Subtitles | tick box | `movieSubtitle` |  | Enables subtitles during the intro cinematic. |
| Show Aggro Percentages | tick box | `threatShowNumeric` |  | Check this to show threat on targets as a percentage. |
| Play Aggro Sounds | tick box | `threatPlaySounds` |  | Enable the playing of alert sounds when aggro advances to a higher level. |
| Colorblind Mode | tick box | `colorblindMode` |  | Displays unit reactions and item qualities in tooltips and adds additional information to several other interfaces. |
| Show Item Level | tick box | `showItemLevel` |  | Display item level in the tooltip for certain items. |
| AggroWarningDisplay | drop-down: Always = `3`; In Instance = `1`; In Party = `2`; Never = `0` | `threatWarning` | | |
| WorldPVPObjectiveDisplay | drop-down: Always = `1`; Dynamic = `2`; Never = `3` | `displayWorldPVPObjectives` | | |

### Objectives

| Setting | Kind | Stored name | Needs | What the original says |
|---|---|---|---|---|
| Instant Quest Text | tick box | `questFadingDisable` |  |  |
| Automatic Quest Tracking | tick box | `autoQuestWatch` |  |  |
| Automatic Quest Progress Updates | tick box | `autoQuestProgress` |  |  |
| Quest Difficulty Color | tick box | `mapQuestDifficulty` |  |  |
| Movable World Map | tick box | `advancedWorldMap` |  |  |
| Wider Objectives Tracker | tick box | `watchFrameWidth` |  |  |

### Social

| Setting | Kind | Stored name | Needs | What the original says |
|---|---|---|---|---|
| Mature Language Filter | tick box | `profanityFilter` |  | Enable mature language filtering. |
| Chat Bubbles | tick box | `chatBubbles` |  |  |
| Party Chat Bubbles | tick box | `chatBubblesParty` |  |  |
| Disable Spam Filter | tick box | `spamFilter` |  | Disables spam filtering on chat text. |
| Remove Chat Hover Delay | tick box | `removeChatDelay` |  |  |
| Guild Member Alert | tick box | `guildMemberNotify` |  | Show a message when your guild members login and logout. |
| Auto Join Guild Recruitment Channel | tick box | `guildRecruitmentChannel` |  | Check this option to automatically join the Guild Recruitment chat channel based upon whether or not you are in a guild. |
| Icons in Chat | tick box | `showChatIcons` |  | Controls whether or not icon links in auto-joined chat channels (such as Trade) are displayed as icons. |
| Click to Focus | tick box | `wholeChatWindowClickable` |  | Clicking anywhere in a Chat Frame will cause it to receive focus. |
| Enable Mouse Wheel Scrolling | tick box | `chatMouseScroll` |  | Check to make the mouse wheel scroll through chat when the cursor is over a chat window. |
| ChatStyle | drop-down: IM Style = `im`; Classic Style = `classic` | `chatStyle` | | |
| ConversationMode | drop-down: New Tab = `popout`; In-line = `inline` | `conversationMode` | | |
| Timestamps | drop-down: computed by the original at run time (from the machine or the account) | `showTimestamps` | | |

### ActionBars

| Setting | Kind | Stored name | Needs | What the original says |
|---|---|---|---|---|
| Bottom Left Bar | tick box | `bottomLeftActionBar` |  |  |
| Bottom Right Bar | tick box | `bottomRightActionBar` |  |  |
| Right Bar | tick box | `rightActionBar` |  |  |
| Right Bar 2 | tick box | `rightTwoActionBar` |  |  |
| Lock ActionBars | tick box | `lockActionBars` |  |  |
| Always Show ActionBars | tick box | `alwaysShowActionBars` |  |  |
| Secure Ability Toggle | tick box | `secureAbilityToggle` |  | When selected you will be protected from toggling your abilities off if accidently hitting the button more than once in a short period of time. |

### Names

| Setting | Kind | Stored name | Needs | What the original says |
|---|---|---|---|---|
| My Name | tick box | `UnitNameOwn` |  | Display your character's name in the game world. |
| NPC Names | tick box | `UnitNameNPC` |  | Show NPC names in the game world. |
| Critters and Vanity Pets | tick box | `UnitNameNonCombatCreatureName` |  | Show critter and vanity pet names in the game world. |
| Guild Names | tick box | `UnitNamePlayerGuild` |  | Show players' guild names in the game world. |
| Titles | tick box | `UnitNamePlayerPVPTitle` |  | Show player titles in the game world. |
| Friendly Players | tick box | `UnitNameFriendlyPlayerName` |  | Show friendly players' names in the game world. |
| Pets | tick box | `UnitNameFriendlyPetName` |  | Show friendly pet names in the game world. |
| Guardians | tick box | `UnitNameFriendlyGuardianName` |  | Show friendly guardian names in the game world. |
| Totems | tick box | `UnitNameFriendlyTotemName` |  | Show friendly totem names in the game world. |
| Enemy Players | tick box | `UnitNameEnemyPlayerName` |  | Show enemy players' names in the game world. |
| Pets | tick box | `UnitNameEnemyPetName` |  | Show enemy pet names in the game world. |
| Guardians | tick box | `UnitNameEnemyGuardianName` |  | Show enemy guardian names in the game world. |
| Totems | tick box | `UnitNameEnemyTotemName` |  | Show enemy totem names in the game world. |
| Friendly Units | tick box | `nameplateShowFriends` |  | Turn this on to display Unit Nameplates for friendly units |
| Pets | tick box | `nameplateShowFriendlyPets` |  | Turn this on to display Unit Nameplates for friendly pets. |
| Guardians | tick box | `nameplateShowFriendlyGuardians` |  | Turn this on to display Unit Nameplates for friendly guardians |
| Totems | tick box | `nameplateShowFriendlyTotems` |  | Turn this on to display Unit Nameplates for friendly totems |
| Enemy Units | tick box | `nameplateShowEnemies` |  | Turn this on to display Unit Nameplates for enemies. |
| Pets | tick box | `nameplateShowEnemyPets` |  | Turn this on to display Unit Nameplates for enemy pets |
| Guardians | tick box | `nameplateShowEnemyGuardians` |  | Turn this on to display Unit Nameplates for enemy guardians |
| Totems | tick box | `nameplateShowEnemyTotems` |  | Turn this on to display Unit Nameplates for enemy totems |
| Allow Overlapping Unit Nameplates | tick box | `nameplateAllowOverlap` |  | Unit Nameplates can overlap if units are visually in a line. Turn this off to keep each Unit Nameplate seperate from each other. |

### Combat Text

| Setting | Kind | Stored name | Needs | What the original says |
|---|---|---|---|---|
| Enable My Floating Combat Text | tick box | `enableCombatText` |  |  |
| Combat State | tick box | `fctCombatState` |  |  |
| Dodges/Parries/Misses | tick box | `fctDodgeParryMiss` |  |  |
| Damage Reduction | tick box | `fctDamageReduction` |  |  |
| Reputation Changes | tick box | `fctRepChanges` |  |  |
| Reactive Spells & Abilities | tick box | `fctReactives` |  |  |
| Friendly Healer Names | tick box | `fctFriendlyHealers` |  |  |
| Combo Points | tick box | `fctComboPoints` |  |  |
| Low Mana & Health | tick box | `fctLowManaHealth` |  |  |
| Energy Gains | tick box | `fctEnergyGains` |  |  |
| Periodic Energy Gains | tick box | `fctPeriodicEnergyGains` |  |  |
| Honor Gained | tick box | `fctHonorGains` |  |  |
| Auras | tick box | `fctAuras` |  |  |
| Target Damage | tick box | `CombatDamage` |  |  |
| Periodic Damage | tick box | `CombatLogPeriodicSpells` |  | Display damage caused by periodic effects such as Rend and Shadow Word: Pain. |
| Pet Damage | tick box | `PetMeleeDamage` |  | Show damage caused by your pet. |
| Healing | tick box | `CombatHealing` |  | Display amount of healing you did to the target. |
| Target Effects | tick box | `fctSpellMechanics` |  | Display effects such as Silence and Snare on your target. |
| Show for other players' targets | tick box | `fctSpellMechanicsOther` |  | Display effects such as Silence and Snare on other players' targets. |
| FCT | drop-down: Scroll Up = `1`; Scroll Down = `2`; Arc = `3` | `combatTextFloatMode` | | |

### Status Text

| Setting | Kind | Stored name | Needs | What the original says |
|---|---|---|---|---|
| Experience Bar Text | tick box | `xpBarText` |  |  |
| Player | tick box | `playerStatusText` |  | Always display the text on your character's health, mana, rage, or energy bars. |
| Pet | tick box | `petStatusText` |  | Always display the text on your pet's health, mana, or focus bars. |
| Party | tick box | `partyStatusText` |  | Always display the text on your party's health, mana, rage, or energy bars. |
| Target | tick box | `targetStatusText` |  | Always display the text on your target's health, mana, rage, energy, or focus bars. |
| Display Percentages | tick box | `statusTextPercentage` |  | Always display status text using percentage values. |

### UnitFrames

| Setting | Kind | Stored name | Needs | What the original says |
|---|---|---|---|---|
| Party/Arena Background | tick box | `showPartyBackground` |  |  |
| Hide Party Interface in Raid | tick box | `hidePartyInRaid` |  |  |
| Party Members' Pets | tick box | `showPartyPets` |  |  |
| Range in Raid Interface | tick box | `showRaidRange` |  |  |
| Arena Enemy Frames | tick box | `showArenaEnemyFrames` |  |  |
| Show Cast Bars | tick box | `showArenaEnemyCastbar` |  |  |
| Show Pets | tick box | `showArenaEnemyPets` |  |  |
| Larger Focus Frame | tick box | `fullSizeFocusFrame` |  |  |

### Buffs and Debuffs

| Setting | Kind | Stored name | Needs | What the original says |
|---|---|---|---|---|
| Buff Durations | tick box | `buffDurations` |  |  |
| Dispellable Debuffs | tick box | `showDispelDebuffs` |  |  |
| Castable Buffs | tick box | `showCastableBuffs` |  |  |
| Consolidate Buffs | tick box | `consolidateBuffs` |  |  |
| Castable Debuffs | tick box | `showCastableDebuffs` |  |  |

### Battle.net

| Setting | Kind | Stored name | Needs | What the original says |
|---|---|---|---|---|
| Online Friends | tick box | `showToastOnline` |  |  |
| Offline Friends | tick box | `showToastOffline` |  |  |
| Broadcast Updates | tick box | `showToastBroadcast` |  |  |
| Real ID Friend Requests | tick box | `showToastFriendRequest` |  |  |
| Conversation Notifications | tick box | `showToastConversation` |  |  |
| Show Toast Window | tick box | `showToastWindow` |  |  |
| Toast Duration | slider 0 to 10, step 0.5 | `toastDuration` |  |  |

### Camera

| Setting | Kind | Stored name | Needs | What the original says |
|---|---|---|---|---|
| Follow Terrain | tick box | `cameraTerrainTilt` |  | Automatically change the camera angle based on the terrain. If your character climbs a slope, the camera will rotate upwards; if you descend, it will rotate downwards. |
| Head Bob | tick box | `cameraBobbing` |  | Simulate head bobbing when in first-person perspective. |
| Water Collision | tick box | `cameraWaterCollision` |  | Set the camera to be above water when your character is above water, and below water when your character is below water. |
| Smart Pivot | tick box | `cameraPivot` |  | Lets you free look when the camera is on the ground. |
| Auto Follow Speed | slider 90 to 270, step 10 | `cameraYawSmoothSpeed` |  | Adjust the camera movement speed in Always and Smart following styles. |
| Max Camera Distance | slider 1 to 2, step 0.1 | `cameraDistanceMaxFactor` |  | Adjust the maximum distance the camera will follow behind you. |
| Style | drop-down: Only horizontal when moving = `1`; Only when moving = `4`; Always adjust camera = `2`; Never adjust camera = `0` | `cameraSmoothStyle` | | |

### Mouse

| Setting | Kind | Stored name | Needs | What the original says |
|---|---|---|---|---|
| Invert Mouse | tick box | `mouseInvertPitch` |  | Invert the way that the mouse affects camera pitch. |
| Detect World of Warcraft Gaming Mouse | tick box | `enableWoWMouse` |  | Enable this option if you have a World of Warcraft Gaming Mouse and you wish to bind keys to the extra buttons. |
| Click-to-Move | tick box | `autointeract` |  | Use mouse clicks to move your character to destinations. |
| Mouse Sensitivity | slider 0.5 to 1.5, step 0.05 | `mouseSpeed` |  | Adjust the speed at which the mouse cursor moves. |
| Mouse Look Speed | slider 90 to 270, step 10 | `cameraYawMoveSpeed` |  | Adjust the speed at which you turn the camera when using mouse look. |
| ClickMoveStyle | drop-down: Only horizontal when moving = `1`; Only when moving = `4`; Always adjust camera = `2`; Never adjust camera = `0` | `cameraSmoothTrackingStyle` | | |

### Features

| Setting | Kind | Stored name | Needs | What the original says |
|---|---|---|---|---|
| Use Equipment Manager | tick box | `equipmentManager` |  |  |
| Preview Talent Changes | tick box | `previewTalents` |  |  |

### Help

| Setting | Kind | Stored name | Needs | What the original says |
|---|---|---|---|---|
| Tutorials | tick box | `showTutorials` |  | Display tutorials which help introduce you to WoW. |
| Loading Screen Tips | tick box | `showGameTips` |  |  |
| Enhanced Tooltips | tick box | `UberTooltips` |  | Enable detailed tooltips in the lower right hand corner of the screen. |
| Beginner Tooltips | tick box | `showNewbieTips` |  |  |
| Display Lua Errors | tick box | `scriptErrors` |  | Shows error messages related to UI functionality. |

### Languages

| Setting | Kind | Stored name | Needs | What the original says |
|---|---|---|---|---|
| Use English Audio | tick box | `useEnglishAudio` |  | Check this box to override your language setting and use english audio. |
| Locale | drop-down: computed by the original at run time (from the machine or the account) | `locale` | | |

## Key Bindings

Every action the original lets a key be bound to, in its own sections and
order. The default key for each lives inside the original program, not
in these files, so none are listed here.

### Movement Keys

Move and Steer (`MOVEANDSTEER`) · Move Forward (`MOVEFORWARD`) · Move Backward (`MOVEBACKWARD`) · Turn Left (`TURNLEFT`) · Turn Right (`TURNRIGHT`) · Strafe Left (`STRAFELEFT`) · Strafe Right (`STRAFERIGHT`) · Jump (`JUMP`) · Sit/Move Down (`SITORSTAND`) · Sheath/Unsheath Weapon (`TOGGLESHEATH`) · Toggle Autorun (`TOGGLEAUTORUN`) · Pitch Up (`PITCHUP`) · Pitch Down (`PITCHDOWN`) · Toggle Run/Walk (`TOGGLERUN`) · Follow Target (`FOLLOWTARGET`)

### Chat Functions

Open Chat (`OPENCHAT`) · Open Chat Slash (`OPENCHATSLASH`) · Chat Page Up (`CHATPAGEUP`) · Chat Page Down (`CHATPAGEDOWN`) · Chat Bottom (`CHATBOTTOM`) · Chat Reply (`REPLY`) · Re-Whisper (`REPLY2`) · Combat Log Page Up (`COMBATLOGPAGEUP`) · Combat Log Page Down (`COMBATLOGPAGEDOWN`) · Combat Log Bottom (`COMBATLOGBOTTOM`)

### Action Bar Functions

Action Button 1 (`ACTIONBUTTON1`) · Action Button 2 (`ACTIONBUTTON2`) · Action Button 3 (`ACTIONBUTTON3`) · Action Button 4 (`ACTIONBUTTON4`) · Action Button 5 (`ACTIONBUTTON5`) · Action Button 6 (`ACTIONBUTTON6`) · Action Button 7 (`ACTIONBUTTON7`) · Action Button 8 (`ACTIONBUTTON8`) · Action Button 9 (`ACTIONBUTTON9`) · Action Button 10 (`ACTIONBUTTON10`) · Action Button 11 (`ACTIONBUTTON11`) · Action Button 12 (`ACTIONBUTTON12`) · Special Action Button 1 (`SHAPESHIFTBUTTON1`) · Special Action Button 2 (`SHAPESHIFTBUTTON2`) · Special Action Button 3 (`SHAPESHIFTBUTTON3`) · Special Action Button 4 (`SHAPESHIFTBUTTON4`) · Special Action Button 5 (`SHAPESHIFTBUTTON5`) · Special Action Button 6 (`SHAPESHIFTBUTTON6`) · Special Action Button 7 (`SHAPESHIFTBUTTON7`) · Special Action Button 8 (`SHAPESHIFTBUTTON8`) · Special Action Button 9 (`SHAPESHIFTBUTTON9`) · Special Action Button 10 (`SHAPESHIFTBUTTON10`) · Secondary Action Button 1 (`BONUSACTIONBUTTON1`) · Secondary Action Button 2 (`BONUSACTIONBUTTON2`) · Secondary Action Button 3 (`BONUSACTIONBUTTON3`) · Secondary Action Button 4 (`BONUSACTIONBUTTON4`) · Secondary Action Button 5 (`BONUSACTIONBUTTON5`) · Secondary Action Button 6 (`BONUSACTIONBUTTON6`) · Secondary Action Button 7 (`BONUSACTIONBUTTON7`) · Secondary Action Button 8 (`BONUSACTIONBUTTON8`) · Secondary Action Button 9 (`BONUSACTIONBUTTON9`) · Secondary Action Button 10 (`BONUSACTIONBUTTON10`) · Action Page 1 (`ACTIONPAGE1`) · Action Page 2 (`ACTIONPAGE2`) · Action Page 3 (`ACTIONPAGE3`) · Action Page 4 (`ACTIONPAGE4`) · Action Page 5 (`ACTIONPAGE5`) · Action Page 6 (`ACTIONPAGE6`) · Previous Action Bar (`PREVIOUSACTIONPAGE`) · Next Action Bar (`NEXTACTIONPAGE`) · Toggle ActionBar Lock (`TOGGLEACTIONBARLOCK`) · Toggle Auto Self Cast (`TOGGLEAUTOSELFCAST`)

### Shaman Totem Bar Functions

Call of the Elements (`MULTICASTSUMMONBUTTON1`) · Earth Totem (`MULTICASTACTIONBUTTON1`) · Fire Totem (`MULTICASTACTIONBUTTON2`) · Water Totem (`MULTICASTACTIONBUTTON3`) · Air Totem (`MULTICASTACTIONBUTTON4`)

Call of the Ancestors (`MULTICASTSUMMONBUTTON2`) · Earth Totem (`MULTICASTACTIONBUTTON5`) · Fire Totem (`MULTICASTACTIONBUTTON6`) · Water Totem (`MULTICASTACTIONBUTTON7`) · Air Totem (`MULTICASTACTIONBUTTON8`)

Call of the Spirits (`MULTICASTSUMMONBUTTON3`) · Earth Totem (`MULTICASTACTIONBUTTON9`) · Fire Totem (`MULTICASTACTIONBUTTON10`) · Water Totem (`MULTICASTACTIONBUTTON11`) · Air Totem (`MULTICASTACTIONBUTTON12`)

Totemic Recall (`MULTICASTRECALLBUTTON1`)

### Targeting Functions

Target Nearest Enemy (`TARGETNEARESTENEMY`) · Target Previous Enemy (`TARGETPREVIOUSENEMY`) · Target Nearest Friend (`TARGETNEARESTFRIEND`) · Target Previous Friend (`TARGETPREVIOUSFRIEND`) · Target Nearest Enemy Player (`TARGETNEARESTENEMYPLAYER`) · Target Previous Enemy Player (`TARGETPREVIOUSENEMYPLAYER`) · Target Nearest Friendly Player (`TARGETNEARESTFRIENDPLAYER`) · Target Previous Friendly Player (`TARGETPREVIOUSFRIENDPLAYER`) · TARGETNEAREST (`TARGETNEAREST`) · TARGETPREVIOUS (`TARGETPREVIOUS`) · Target Self (`TARGETSELF`) · Target Party Member 1 (`TARGETPARTYMEMBER1`) · Target Party Member 2 (`TARGETPARTYMEMBER2`) · Target Party Member 3 (`TARGETPARTYMEMBER3`) · Target Party Member 4 (`TARGETPARTYMEMBER4`) · Target Pet (`TARGETPET`) · Target Party Pet 1 (`TARGETPARTYPET1`) · Target Party Pet 2 (`TARGETPARTYPET2`) · Target Party Pet 3 (`TARGETPARTYPET3`) · Target Party Pet 4 (`TARGETPARTYPET4`) · Target Last Hostile (`TARGETLASTHOSTILE`) · Target Last Target (`TARGETLASTTARGET`) · Show Enemy Name Plates (`NAMEPLATES`) · Show Friendly Name Plates (`FRIENDNAMEPLATES`) · Show All Name Plates (`ALLNAMEPLATES`) · Interact With Mouseover (`INTERACTMOUSEOVER`) · Interact With Target (`INTERACTTARGET`) · Assist Target (`ASSISTTARGET`) · Attack Target (`ATTACKTARGET`) · Start Attack (`STARTATTACK`) · Pet Attack (`PETATTACK`) · Focus Target (`FOCUSTARGET`) · Target Focus (`TARGETFOCUS`) · Target Mouseover (`TARGETMOUSEOVER`) · Target Current Talker (`TARGETTALKER`)

### Interface Panel Functions

Toggle Character Pane (`TOGGLECHARACTER0`) · Toggle Backpack (`TOGGLEBACKPACK`) · Toggle Bag 1 (`TOGGLEBAG1`) · Toggle Bag 2 (`TOGGLEBAG2`) · Toggle Bag 3 (`TOGGLEBAG3`) · Toggle Bag 4 (`TOGGLEBAG4`) · Open All Bags (`OPENALLBAGS`) · Toggle Keyring (`TOGGLEKEYRING`) · Toggle Spellbook (`TOGGLESPELLBOOK`) · Toggle Pet Book (`TOGGLEPETBOOK`) · Toggle Glyphs (`TOGGLEINSCRIPTION`) · Toggle Talent Pane (`TOGGLETALENTS`) · Toggle PVP Pane (`TOGGLECHARACTER4`) · Toggle Pet Pane (`TOGGLECHARACTER3`) · Toggle Reputation Pane (`TOGGLECHARACTER2`) · Toggle Skill Pane (`TOGGLECHARACTER1`) · Toggle Quest Log (`TOGGLEQUESTLOG`) · Toggle Game Menu (`TOGGLEGAMEMENU`) · Toggle Minimap (`TOGGLEMINIMAP`) · Toggle World Map Pane (`TOGGLEWORLDMAP`) · Toggle World Map Size (`TOGGLEWORLDMAPSIZE`) · Toggle Social Pane (`TOGGLESOCIAL`) · Toggle Friends Pane (`TOGGLEFRIENDSTAB`) · Toggle Who Pane (`TOGGLEWHOTAB`) · Toggle Guild Pane (`TOGGLEGUILDTAB`) · Toggle Chat Pane (`TOGGLECHATTAB`) · Toggle Raid Pane (`TOGGLERAIDTAB`) · Toggle Dungeon Finder Frame (`TOGGLELFGPARENT`) · Toggle Raid Browser (`TOGGLELFRPARENT`) · Toggle Score Screen (`TOGGLEWORLDSTATESCORES`) · Toggle Zone Map (`TOGGLEBATTLEFIELDMINIMAP`) · Toggle Minimap Rotation (`TOGGLEMINIMAPROTATION`) · Toggle Channel Pullout (`TOGGLECHANNELPULLOUT`) · Toggle Achievement Frame (`TOGGLEACHIEVEMENT`) · Toggle Statistics Frame (`TOGGLESTATISTICS`) · Toggle Currency Frame (`TOGGLECURRENCY`)

### Miscellaneous Functions

Stop Casting (`STOPCASTING`) · Stop Attacking (`STOPATTACK`) · Dismount (`DISMOUNT`) · Minimap Zoom In (`MINIMAPZOOMIN`) · Minimap Zoom Out (`MINIMAPZOOMOUT`) · Toggle Music (`TOGGLEMUSIC`) · Toggle Sound (`TOGGLESOUND`) · Master Volume Up (`MASTERVOLUMEUP`) · Master Volume Down (`MASTERVOLUMEDOWN`) · Toggle Self Mute (`TOGGLESELFMUTE`) · Toggle User Interface (`TOGGLEUI`) · Toggle Framerate Display (`TOGGLEFPS`) · Screen Shot (`SCREENSHOT`) · TOGGLESTATS (`TOGGLESTATS`) · TOGGLETRIS (`TOGGLETRIS`) · TOGGLEPORTALS (`TOGGLEPORTALS`) · TOGGLECOLLISION (`TOGGLECOLLISION`) · TOGGLECOLLISIONDISPLAY (`TOGGLECOLLISIONDISPLAY`) · TOGGLEPLAYERBOUNDS (`TOGGLEPLAYERBOUNDS`) · TOGGLEPERFORMANCEDISPLAY (`TOGGLEPERFORMANCEDISPLAY`) · TOGGLEPERFORMANCEPAUSE (`TOGGLEPERFORMANCEPAUSE`) · TOGGLEPERFORMANCEVALUES (`TOGGLEPERFORMANCEVALUES`) · RESETPERFORMANCEVALUES (`RESETPERFORMANCEVALUES`) · TOGGLECOMMENTATOR (`TOGGLECOMMENTATOR`)

### Camera Functions

Next View (`NEXTVIEW`) · Previous View (`PREVVIEW`) · Zoom In (`CAMERAZOOMIN`) · Zoom Out (`CAMERAZOOMOUT`) · Set View 1 (`SETVIEW1`) · Set View 2 (`SETVIEW2`) · Set View 3 (`SETVIEW3`) · Set View 4 (`SETVIEW4`) · Set View 5 (`SETVIEW5`) · Save View 1 (`SAVEVIEW1`) · Save View 2 (`SAVEVIEW2`) · Save View 3 (`SAVEVIEW3`) · Save View 4 (`SAVEVIEW4`) · Save View 5 (`SAVEVIEW5`) · Reset View 1 (`RESETVIEW1`) · Reset View 2 (`RESETVIEW2`) · Reset View 3 (`RESETVIEW3`) · Reset View 4 (`RESETVIEW4`) · Reset View 5 (`RESETVIEW5`) · Flip Camera (`FLIPCAMERAYAW`)

### MultiActionBar Bindings

BottomLeft Action Button 1 (`MULTIACTIONBAR1BUTTON1`) · BottomLeft Action Button 2 (`MULTIACTIONBAR1BUTTON2`) · BottomLeft Action Button 3 (`MULTIACTIONBAR1BUTTON3`) · BottomLeft Action Button 4 (`MULTIACTIONBAR1BUTTON4`) · BottomLeft Action Button 5 (`MULTIACTIONBAR1BUTTON5`) · BottomLeft Action Button 6 (`MULTIACTIONBAR1BUTTON6`) · BottomLeft Action Button 7 (`MULTIACTIONBAR1BUTTON7`) · BottomLeft Action Button 8 (`MULTIACTIONBAR1BUTTON8`) · BottomLeft Action Button 9 (`MULTIACTIONBAR1BUTTON9`) · BottomLeft Action Button 10 (`MULTIACTIONBAR1BUTTON10`) · BottomLeft Action Button 11 (`MULTIACTIONBAR1BUTTON11`) · BottomLeft Action Button 12 (`MULTIACTIONBAR1BUTTON12`)

BottomRight Action Button 1 (`MULTIACTIONBAR2BUTTON1`) · BottomRight Action Button 2 (`MULTIACTIONBAR2BUTTON2`) · BottomRight Action Button 3 (`MULTIACTIONBAR2BUTTON3`) · BottomRight Action Button 4 (`MULTIACTIONBAR2BUTTON4`) · BottomRight Action Button 5 (`MULTIACTIONBAR2BUTTON5`) · BottomRight Action Button 6 (`MULTIACTIONBAR2BUTTON6`) · BottomRight Action Button 7 (`MULTIACTIONBAR2BUTTON7`) · BottomRight Action Button 8 (`MULTIACTIONBAR2BUTTON8`) · BottomRight Action Button 9 (`MULTIACTIONBAR2BUTTON9`) · BottomRight Action Button 10 (`MULTIACTIONBAR2BUTTON10`) · BottomRight Action Button 11 (`MULTIACTIONBAR2BUTTON11`) · BottomRight Action Button 12 (`MULTIACTIONBAR2BUTTON12`)

Right Action Button 1 (`MULTIACTIONBAR3BUTTON1`) · Right Action Button 2 (`MULTIACTIONBAR3BUTTON2`) · Right Action Button 3 (`MULTIACTIONBAR3BUTTON3`) · Right Action Button 4 (`MULTIACTIONBAR3BUTTON4`) · Right Action Button 5 (`MULTIACTIONBAR3BUTTON5`) · Right Action Button 6 (`MULTIACTIONBAR3BUTTON6`) · Right Action Button 7 (`MULTIACTIONBAR3BUTTON7`) · Right Action Button 8 (`MULTIACTIONBAR3BUTTON8`) · Right Action Button 9 (`MULTIACTIONBAR3BUTTON9`) · Right Action Button 10 (`MULTIACTIONBAR3BUTTON10`) · Right Action Button 11 (`MULTIACTIONBAR3BUTTON11`) · Right Action Button 12 (`MULTIACTIONBAR3BUTTON12`)

Right ActionBar 2 Button 1 (`MULTIACTIONBAR4BUTTON1`) · Right ActionBar 2 Button 2 (`MULTIACTIONBAR4BUTTON2`) · Right ActionBar 2 Button 3 (`MULTIACTIONBAR4BUTTON3`) · Right ActionBar 2 Button 4 (`MULTIACTIONBAR4BUTTON4`) · Right ActionBar 2 Button 5 (`MULTIACTIONBAR4BUTTON5`) · Right ActionBar 2 Button 6 (`MULTIACTIONBAR4BUTTON6`) · Right ActionBar 2 Button 7 (`MULTIACTIONBAR4BUTTON7`) · Right ActionBar 2 Button 8 (`MULTIACTIONBAR4BUTTON8`) · Right ActionBar 2 Button 9 (`MULTIACTIONBAR4BUTTON9`) · Right ActionBar 2 Button 10 (`MULTIACTIONBAR4BUTTON10`) · Right ActionBar 2 Button 11 (`MULTIACTIONBAR4BUTTON11`) · Right ActionBar 2 Button 12 (`MULTIACTIONBAR4BUTTON12`)

### Raid Targeting

Assign Star to Target (`RAIDTARGET1`) · Assign Circle to Target (`RAIDTARGET2`) · Assign Diamond to Target (`RAIDTARGET3`) · Assign Triangle to Target (`RAIDTARGET4`) · Assign Moon to Target (`RAIDTARGET5`) · Assign Square to Target (`RAIDTARGET6`) · Assign Cross to Target (`RAIDTARGET7`) · Assign Skull to Target (`RAIDTARGET8`) · Clear Raid Target Icon (`RAIDTARGETNONE`)

### Vehicle Controls

Exit Vehicle (`VEHICLEEXIT`) · Previous Seat (`VEHICLEPREVSEAT`) · Next Seat (`VEHICLENEXTSEAT`) · Aim Up (`VEHICLEAIMUP`) · Aim Down (`VEHICLEAIMDOWN`) · Aim Increment (`VEHICLEAIMINCREMENT`) · Aim Decrement (`VEHICLEAIMDECREMENT`) · Camera Zoom In (`VEHICLECAMERAZOOMIN`) · Camera Zoom Out (`VEHICLECAMERAZOOMOUT`) · TURNORACTION (`TURNORACTION`) · CAMERAORSELECTORMOVE (`CAMERAORSELECTORMOVE`)

### iTunes Remote

iTunes Play/Pause (`ITUNES_PLAYPAUSE`) · iTunes Next Track (`ITUNES_NEXTTRACK`) · iTunes Back Track (`ITUNES_BACKTRACK`) · iTunes Volume Up (`ITUNES_VOLUMEUP`) · iTunes Volume Down (`ITUNES_VOLUMEDOWN`)

### Movie Recording

Start/Stop Recording (`MOVIE_RECORDING_STARTSTOP`) · Cancel Recording/Compression (`MOVIE_RECORDING_CANCEL`) · Compress Movies (`MOVIE_RECORDING_COMPRESS`) · Show/Hide User Interface (`MOVIE_RECORDING_GUI`)

(275 bindable actions.)
