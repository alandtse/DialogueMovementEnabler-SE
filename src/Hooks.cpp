#include "Hooks.h"
#include "AutoCloseManager.h"
#include "Settings.h"

namespace DME
{
#ifdef SKYRIMVR
	// Right/left hand controllers no longer share a single device id; each headset family
	// (Vive/Oculus/WMR) has its own primary (dominant hand) and secondary device id.
	inline bool IsPrimaryHandDevice(RE::INPUT_DEVICE a_device)
	{
		return a_device == RE::INPUT_DEVICES::INPUT_DEVICE::kVivePrimary ||
		       a_device == RE::INPUT_DEVICES::INPUT_DEVICE::kOculusPrimary ||
		       a_device == RE::INPUT_DEVICES::INPUT_DEVICE::kWMRPrimary;
	}

	inline bool IsSecondaryHandDevice(RE::INPUT_DEVICE a_device)
	{
		return a_device == RE::INPUT_DEVICES::INPUT_DEVICE::kViveSecondary ||
		       a_device == RE::INPUT_DEVICES::INPUT_DEVICE::kOculusSecondary ||
		       a_device == RE::INPUT_DEVICES::INPUT_DEVICE::kWMRSecondary;
	}

	// Every frame, RE::PlayerCharacter::UpdateVRComfortCheck (a per-frame VR "comfort radius"
	// Havok collision check, normally a capsule cast around the player's head, radius
	// fPlayerComfortRadiusMeters:VRTeleport) sets/clears playerFlags.vrComfortFadeActive, which
	// drives the screen-fade-to-black. While a dialogue is open, that same function retargets
	// the cast at DialogueMenu::occlusionCheckNode's world position with a different radius
	// (fPlayerDialogueMenuOcclusionCheckSize:VRUI) -- that's the "occlusion" check issue #2 is
	// about. But the *default* (non-dialogue) cast around the player's own head still runs
	// unconditionally in every other case, including when a dialogue is open, and since NPCs
	// stand close to the player during dialogue by design, that default cast alone can trip the
	// same fade.
	//
	// The reliable fix is to skip the whole per-frame check while a dialogue is open, rather
	// than try to selectively neutralize one of its internal branches, and to force the flag to
	// "not occluded" instead of leaving whatever state it was last computed to.
	void PlayerCharacterUpdateVRComfortCheck_Hook(RE::PlayerCharacter* a_player)
	{
		if (Settings::GetSingleton()->disableOcclusionCheck && RE::UI::GetSingleton()->IsMenuOpen(RE::DialogueMenu::MENU_NAME))
		{
			a_player->playerFlags.vrComfortFadeActive = false;
			return;
		}

		a_player->UpdateVRComfortCheck();
	}
#endif

	class MenuControlsEx : public RE::MenuControls
	{
	public:
		using ProcessEvent_t = decltype(static_cast<RE::BSEventNotifyControl (RE::MenuControls::*)(RE::InputEvent* const*, RE::BSTEventSource<RE::InputEvent*>*)>(&RE::MenuControls::ProcessEvent));
		inline static REL::Relocation<ProcessEvent_t> _ProcessEvent;

		bool IsMappedToSameButton(std::uint32_t a_keyMask, RE::INPUT_DEVICE a_deviceType, RE::BSFixedString a_controlName, RE::UserEvents::INPUT_CONTEXT_ID a_context = RE::UserEvents::INPUT_CONTEXT_ID::kGameplay)
		{
			RE::ControlMap* controlMap = RE::ControlMap::GetSingleton();

			if (a_deviceType == RE::INPUT_DEVICE::kKeyboard)
			{
				std::uint32_t keyMask = controlMap->GetMappedKey(a_controlName, RE::INPUT_DEVICE::kKeyboard, a_context);
				return a_keyMask == keyMask;
			}
			else if (a_deviceType == RE::INPUT_DEVICE::kMouse)
			{
				std::uint32_t keyMask = controlMap->GetMappedKey(a_controlName, RE::INPUT_DEVICE::kMouse, a_context);
				return a_keyMask == keyMask;
			}

			return false;
		}

		RE::BSEventNotifyControl ProcessEvent_Hook(RE::InputEvent** a_event, RE::BSTEventSource<RE::InputEvent*>* a_source)
		{
			RE::UI* ui = RE::UI::GetSingleton();
			RE::PlayerControls* pc = RE::PlayerControls::GetSingleton();
			RE::UserEvents* userEvents = RE::UserEvents::GetSingleton();
			#ifndef SKYRIMVR
			RE::BSInputDeviceManager* inputDeviceManager = RE::BSInputDeviceManager::GetSingleton();
			#endif
			// SkyrimSouls compatibility
			using func_t = bool (*)();
			REL::Relocation<func_t> func(REL::ID{ 56476 });
			bool isInMenuMode = func();

			bool movementControlsEnabled;

			RE::ControlMap* controlMap = RE::ControlMap::GetSingleton();
			movementControlsEnabled = pc->movementHandler->IsInputEventHandlingEnabled() && controlMap->IsMovementControlsEnabled();

			if (a_event && *a_event && !this->remapMode && !isInMenuMode && ui->IsMenuOpen(RE::DialogueMenu::MENU_NAME))
			{
				for (RE::InputEvent* evn = *a_event; evn; evn = evn->next)
				{
					if (evn && evn->HasIDCode())
					{
						RE::IDEvent* idEvent = static_cast<RE::IDEvent*>(evn);
						Settings* settings = Settings::GetSingleton();

						#ifndef SKYRIMVR
						Settings::ControlType controlType = inputDeviceManager->IsGamepadEnabled() ? Settings::ControlType::kController : Settings::ControlType::kKeyboardMouse;
						#else
						Settings::ControlType controlType = Settings::ControlType::kController;
						#endif

						// Movement
						if (settings->allowMovement[controlType])
						{
							// WASD
							idEvent->userEvent = IsMappedToSameButton(idEvent->idCode, idEvent->device.get(), userEvents->forward) ? userEvents->forward : idEvent->userEvent;
							idEvent->userEvent = IsMappedToSameButton(idEvent->idCode, idEvent->device.get(), userEvents->back) ? userEvents->back : idEvent->userEvent;
							idEvent->userEvent = IsMappedToSameButton(idEvent->idCode, idEvent->device.get(), userEvents->strafeLeft) ? userEvents->strafeLeft : idEvent->userEvent;
							idEvent->userEvent = IsMappedToSameButton(idEvent->idCode, idEvent->device.get(), userEvents->strafeRight) ? userEvents->strafeRight : idEvent->userEvent;

							// Controllers
							#ifndef SKYRIMVR
							idEvent->userEvent = idEvent->userEvent == userEvents->leftStick ? userEvents->move : idEvent->userEvent;
							#else
							if (settings->rightHandControl)
								idEvent->userEvent = IsSecondaryHandDevice(evn->device.get()) && idEvent->userEvent == userEvents->leftStick ? userEvents->move : idEvent->userEvent;
							else
								idEvent->userEvent = IsPrimaryHandDevice(evn->device.get()) && idEvent->userEvent == userEvents->leftStick ? userEvents->move : idEvent->userEvent;
							#endif
						}

						//Rotation
						#ifdef SKYRIMVR
						if (settings->allowRotation[controlType] && evn->GetEventType() == RE::INPUT_EVENT_TYPE::kThumbstick)
						{
							RE::ThumbstickEvent* thumbEvent = static_cast<RE::ThumbstickEvent*>(evn);
							bool isHorizontal = std::fabs(thumbEvent->xValue) > std::fabs(thumbEvent->yValue) * 2.0f; //Bias towards dialogue option selection 60 vs 30 degrees

							if (isHorizontal)
							{
								if (settings->rightHandControl)
									idEvent->userEvent = IsPrimaryHandDevice(evn->device.get()) && idEvent->userEvent == userEvents->leftStick ? userEvents->look : idEvent->userEvent;
								else
									idEvent->userEvent = IsSecondaryHandDevice(evn->device.get()) && idEvent->userEvent == userEvents->leftStick ? userEvents->look : idEvent->userEvent;
							}
						}
						#endif

						//Run
						if (settings->allowRun[controlType] && IsMappedToSameButton(idEvent->idCode, idEvent->device.get(), userEvents->run))
						{
							idEvent->userEvent = movementControlsEnabled ? userEvents->run : "";
						}

						//Toggle Walk/Run
						if (settings->allowToggleRun[controlType] && IsMappedToSameButton(idEvent->idCode, idEvent->device.get(), userEvents->toggleRun))
						{
							idEvent->userEvent = movementControlsEnabled ? userEvents->toggleRun : "";
						}

						//Jump
						if (settings->allowJump[controlType] && IsMappedToSameButton(idEvent->idCode, idEvent->device.get(), userEvents->jump))
						{
							idEvent->userEvent = movementControlsEnabled ? userEvents->jump : "";
						}

						//Sprint
						if (settings->allowSprint[controlType] && IsMappedToSameButton(idEvent->idCode, idEvent->device.get(), userEvents->sprint))
						{
							idEvent->userEvent = movementControlsEnabled ? userEvents->sprint : "";
						}

						//Toggle POV
						if (settings->allowTogglePOV[controlType] && IsMappedToSameButton(idEvent->idCode, idEvent->device.get(), userEvents->togglePOV))
						{
							idEvent->userEvent = movementControlsEnabled ? userEvents->togglePOV : "";
						}

						//Sneak
						if (settings->allowSneak[controlType] && IsMappedToSameButton(idEvent->idCode, idEvent->device.get(), userEvents->sneak))
						{
							idEvent->userEvent = movementControlsEnabled ? userEvents->sneak : "";
						}

						//Ready weapon
						if (settings->allowReadyWeapon[controlType] && IsMappedToSameButton(idEvent->idCode, idEvent->device.get(), userEvents->readyWeapon))
						{
							idEvent->userEvent = movementControlsEnabled ? userEvents->readyWeapon : "";
						}

						//Left attack
						if (settings->allowLeftAttack[controlType] && IsMappedToSameButton(idEvent->idCode, idEvent->device.get(), userEvents->leftAttack))
						{
							idEvent->userEvent = movementControlsEnabled ? userEvents->leftAttack : "";
						}

						//Right attack
						if (settings->allowRightAttack[controlType] && IsMappedToSameButton(idEvent->idCode, idEvent->device.get(), userEvents->rightAttack))
						{
							idEvent->userEvent = movementControlsEnabled ? userEvents->rightAttack : "";
						}

						//Shout
						if (settings->allowShout[controlType] && IsMappedToSameButton(idEvent->idCode, idEvent->device.get(), userEvents->shout))
						{
							idEvent->userEvent = movementControlsEnabled ? userEvents->shout : "";
						}

						//Hotkeys
						if (settings->allowHotkeys[controlType])
						{
							idEvent->userEvent = IsMappedToSameButton(idEvent->idCode, idEvent->device.get(), userEvents->hotkey1) ? userEvents->hotkey1 : idEvent->userEvent;
							idEvent->userEvent = IsMappedToSameButton(idEvent->idCode, idEvent->device.get(), userEvents->hotkey2) ? userEvents->hotkey2 : idEvent->userEvent;
							idEvent->userEvent = IsMappedToSameButton(idEvent->idCode, idEvent->device.get(), userEvents->hotkey3) ? userEvents->hotkey3 : idEvent->userEvent;
							idEvent->userEvent = IsMappedToSameButton(idEvent->idCode, idEvent->device.get(), userEvents->hotkey4) ? userEvents->hotkey4 : idEvent->userEvent;
							idEvent->userEvent = IsMappedToSameButton(idEvent->idCode, idEvent->device.get(), userEvents->hotkey5) ? userEvents->hotkey5 : idEvent->userEvent;
							idEvent->userEvent = IsMappedToSameButton(idEvent->idCode, idEvent->device.get(), userEvents->hotkey6) ? userEvents->hotkey6 : idEvent->userEvent;
							idEvent->userEvent = IsMappedToSameButton(idEvent->idCode, idEvent->device.get(), userEvents->hotkey7) ? userEvents->hotkey7 : idEvent->userEvent;
							idEvent->userEvent = IsMappedToSameButton(idEvent->idCode, idEvent->device.get(), userEvents->hotkey8) ? userEvents->hotkey8 : idEvent->userEvent;
						}

						//Disable cancel button
						if (settings->disableCancelButton && idEvent->userEvent == userEvents->cancel)
						{
							idEvent->userEvent = "";
						}

					}
				}
			}

			return _ProcessEvent(this, a_event, a_source);
		}
	};

	class DialogueMenuEx : public RE::DialogueMenu
	{
	public:
		using ProcessMessage_t = decltype(&RE::DialogueMenu::ProcessMessage);
		inline static REL::Relocation<ProcessMessage_t> _ProcessMessage;

		using AdvanceMovie_t = decltype(&RE::DialogueMenu::AdvanceMovie);
		inline static REL::Relocation<AdvanceMovie_t> _AdvanceMovie;

		RE::UI_MESSAGE_RESULTS ProcessMessage_Hook(RE::UIMessage& a_message)
		{
			switch (a_message.type.get())
			{
			case RE::UI_MESSAGE_TYPE::kShow:
				{
					RE::MenuTopicManager* topicManager = RE::MenuTopicManager::GetSingleton();

					RE::TESObjectREFR* target = nullptr;

					if (topicManager->speaker && topicManager->speaker.get())
					{
						target = topicManager->speaker.get().get();
					}
					else if (topicManager->lastSpeaker && topicManager->lastSpeaker.get())
					{
						target = topicManager->lastSpeaker.get().get();
					}
					else
					{
						break;
					}

					RE::PlayerCharacter* player = RE::PlayerCharacter::GetSingleton();

					if (player->GetParentCell()->IsInteriorCell() && target->GetParentCell()->IsInteriorCell())
					{
						if (player->GetParentCell() == target->GetParentCell())
						{
							AutoCloseManager::GetSingleton()->InitAutoClose(target);
						}
					}
					else if (player->GetParentCell()->IsExteriorCell() && target->GetParentCell()->IsExteriorCell())
					{
						if (player->GetWorldspace() == target->GetWorldspace())
						{
							AutoCloseManager::GetSingleton()->InitAutoClose(target);
						}
					}
					else
					{
						AutoCloseManager::GetSingleton()->InitAutoClose(nullptr);
					}
				}
				break;
			}

			return _ProcessMessage(this, a_message);
		}

		void AdvanceMovie_Hook(float a_interval, std::uint32_t a_currentTime)
		{
			AutoCloseManager::GetSingleton()->CheckAutoClose();
			return _AdvanceMovie(this, a_interval, a_currentTime);
		}

	};

	void InstallHooks()
	{

		REL::Relocation<std::uintptr_t> vTable_mc(RE::VTABLE_MenuControls[0]);
		MenuControlsEx::_ProcessEvent = vTable_mc.write_vfunc(0x1, &MenuControlsEx::ProcessEvent_Hook);

		//Hook ProcessMessage
		REL::Relocation<std::uintptr_t> vTable_dm(RE::VTABLE_DialogueMenu[0]);
		DialogueMenuEx::_ProcessMessage = vTable_dm.write_vfunc(0x4, &DialogueMenuEx::ProcessMessage_Hook);
		DialogueMenuEx::_AdvanceMovie = vTable_dm.write_vfunc(0x5, &DialogueMenuEx::AdvanceMovie_Hook);

#ifdef SKYRIMVR
		if (Settings::GetSingleton()->disableOcclusionCheck)
		{
			// Redirect the sole call site of RE::PlayerCharacter::UpdateVRComfortCheck (RE'd via
			// Ghidra) to PlayerCharacterUpdateVRComfortCheck_Hook, which no-ops the whole check
			// while a dialogue is open instead of trying to neutralize just its dialogue-specific
			// branch.
			REL::Relocation<std::uintptr_t> comfortCheckCallSite{ REL::Offset(0x6B53CE) };
			comfortCheckCallSite.write_call<5>(&PlayerCharacterUpdateVRComfortCheck_Hook);
		}
#endif

		#ifndef SKYRIMVR //VR doesn't have this function
		Settings* settings = Settings::GetSingleton();
		if (settings->unlockCamera)
		{
			std::uint8_t buf[] = { 0xE9, 0xB1, 0x00, 0x00, 0x00, 0x90 };  //jmp + nop
			REL::safe_write(REL::ID{ 41292 }.address() + 0x25, std::span<uint8_t>(buf));
		}
		#endif
	}
}
