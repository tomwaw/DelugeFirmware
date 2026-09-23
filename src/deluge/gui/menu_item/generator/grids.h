#pragma once
#include "gui/menu_item/integer.h"
#include "gui/menu_item/submenu.h"
#include "gui/menu_item/toggle.h"
#include "gui/ui_timer_manager.h"
#include "gui/views/instrument_clip_view.h"
#include "hid/display/oled.h"
#include "model/clip/instrument_clip.h"
#include "model/drum/drum.h"
#include "model/note/note_row.h"
#include "model/song/song.h"
#include "playback/playback_handler.h"
#include "util/functions.h"

namespace deluge::gui::menu_item::generator::grids {
inline InstrumentClip& clip() {
	return *getCurrentInstrumentClip();
}
class Menu final : public Submenu {
public:
	using Submenu::Submenu;
	bool isRelevant(ModControllableAudio*, int32_t) const override {
		auto* current = getCurrentInstrumentClip();
		return current && current->output
		       && (current->output->type == OutputType::KIT || current->output->type == OutputType::MIDI_OUT);
	}
	std::string_view getTitle() const override {
		if (title != l10n::String::STRING_FOR_GRIDS)
			return l10n::getView(title);
		if (clip().gridsEnabled && !clip().gridsAvailable())
			return l10n::getView(l10n::String::STRING_FOR_GRIDS_PAUSED);
		return l10n::getView(clip().gridsPending() ? l10n::String::STRING_FOR_GRIDS_PENDING : title);
	}
	void beginSession(MenuItem* from = nullptr) override {
		clip().initializeGridsMapping();
		Submenu::beginSession(from);
		uiTimerManager.setTimer(TimerName::UI_SPECIFIC, 200);
	}
	ActionResult timerCallback() override {
		renderUIsForOled();
		uiTimerManager.setTimer(TimerName::UI_SPECIFIC, 200);
		return ActionResult::DEALT_WITH;
	}
};
class Enabled final : public Toggle {
public:
	using Toggle::Toggle;
	void readCurrentValue() override { setValue(clip().gridsEnabled); }
	void writeCurrentValue() override {
		char memory[MODEL_STACK_MAX_SIZE];
		clip().setGridsEnabled(currentSong->setupModelStackWithCurrentClip(memory), getValue());
		readCurrentValue();
	}
	MenuItem* selectButtonPress() override {
		clip().initializeGridsMapping();
		if (!clip().gridsEnabled && !clip().gridsAvailable()) {
			display->displayPopup(l10n::get(l10n::String::STRING_FOR_GRIDS_UNSUPPORTED));
			return NO_NAVIGATION;
		}
		return Toggle::selectButtonPress();
	}
};
enum class Parameter { X, Y, KICK, SNARE, HAT, CHAOS, SEED };
class Control final : public Integer {
public:
	Control(l10n::String name, Parameter parameter) : Integer(name), parameter_(parameter) {}
	void readCurrentValue() override {
		const auto& settings = clip().gridsSettings;
		switch (parameter_) {
		case Parameter::X:
			setValue(settings.x);
			break;
		case Parameter::Y:
			setValue(settings.y);
			break;
		case Parameter::KICK:
			setValue(settings.density[0]);
			break;
		case Parameter::SNARE:
			setValue(settings.density[1]);
			break;
		case Parameter::HAT:
			setValue(settings.density[2]);
			break;
		case Parameter::CHAOS:
			setValue(settings.chaos);
			break;
		case Parameter::SEED:
			setValue(settings.seed);
			break;
		}
	}
	void writeCurrentValue() override {
		auto& settings = clip().gridsSettings;
		switch (parameter_) {
		case Parameter::X:
			settings.x = getValue();
			break;
		case Parameter::Y:
			settings.y = getValue();
			break;
		case Parameter::KICK:
			settings.density[0] = getValue();
			break;
		case Parameter::SNARE:
			settings.density[1] = getValue();
			break;
		case Parameter::HAT:
			settings.density[2] = getValue();
			break;
		case Parameter::CHAOS:
			settings.chaos = getValue();
			break;
		case Parameter::SEED:
			settings.seed = getValue();
			break;
		}
		clip().refreshGrids();
	}

protected:
	int32_t getMinValue() const override { return 0; }
	int32_t getMaxValue() const override { return parameter_ == Parameter::SEED ? 9999 : 255; }

private:
	Parameter parameter_;
};
class Destination final : public Integer {
public:
	Destination(l10n::String name, uint8_t part, bool kit) : Integer(name), part_(part), kit_(kit) {}
	bool isRelevant(ModControllableAudio*, int32_t) const override {
		return clip().output->type == (kit_ ? OutputType::KIT : OutputType::MIDI_OUT);
	}
	void readCurrentValue() override {
		clip().initializeGridsMapping();
		setValue(kit_ ? clip().gridsRowIndex(part_) + 1 : clip().gridsMidiNotes[part_]);
	}
	void selectEncoderAction(int32_t offset) override {
		if (offset == 0)
			return;
		const int32_t direction = offset > 0 ? 1 : -1;
		int32_t candidate = std::clamp(getValue() + offset, getMinValue(), getMaxValue());
		// Skip occupied destinations instead of trapping the encoder at the adjacent assigned row/note.
		while (candidate >= getMinValue() && candidate <= getMaxValue()) {
			bool occupied = false;
			for (uint8_t other = 0; other < 3; ++other) {
				if (other == part_)
					continue;
				const int32_t assigned = kit_ ? clip().gridsRowIndex(other) + 1 : clip().gridsMidiNotes[other];
				occupied |= candidate == assigned && (!kit_ || candidate != 0);
			}
			if (kit_ && candidate > 0 && !clip().noteRows.getElement(candidate - 1)->drum)
				occupied = true;
			if (!occupied) {
				Integer::selectEncoderAction(candidate - getValue());
				return;
			}
			candidate += direction;
		}
	}

	void writeCurrentValue() override {
		char memory[MODEL_STACK_MAX_SIZE];
		if (!clip().setGridsDestination(currentSong->setupModelStackWithCurrentClip(memory), part_,
		                                getValue() - (kit_ ? 1 : 0)))
			display->displayPopup(l10n::get(l10n::String::STRING_FOR_GRIDS_DUPLICATE));
		readCurrentValue();
	}

protected:
	int32_t getMinValue() const override { return 0; }
	int32_t getMaxValue() const override { return kit_ ? clip().noteRows.getNumElements() : 127; }
	void drawPixelsForOled() override {
		Integer::drawPixelsForOled();
		if (!kit_)
			return;
		const int32_t index = clip().gridsRowIndex(part_);
		const char* label = l10n::get(l10n::String::STRING_FOR_GRIDS_UNASSIGNED);
		if (index >= 0)
			label = clip().noteRows.getElement(index)->drum->drumName.c_str();
		hid::display::OLED::main.drawStringCentred(label, OLED_MAIN_HEIGHT_PIXELS - 8, kTextSpacingX,
		                                           kTextSizeYUpdated);
	}

private:
	uint8_t part_;
	bool kit_;
};
class NewSeed final : public MenuItem {
public:
	using MenuItem::MenuItem;
	bool shouldEnterSubmenu() override { return false; }
	MenuItem* selectButtonPress() override {
		const uint32_t random = (getRandom255() << 8) | getRandom255();
		clip().gridsSettings.seed = (clip().gridsSettings.seed + 1 + random % 9999) % 10000;
		clip().refreshGrids();
		char text[12];
		intToString(clip().gridsSettings.seed, text);
		display->displayPopup(text);
		return NO_NAVIGATION;
	}
};
class Freeze final : public MenuItem {
public:
	using MenuItem::MenuItem;
	bool isRelevant(ModControllableAudio*, int32_t) const override { return clip().output->type == OutputType::KIT; }
	bool shouldEnterSubmenu() override { return false; }
	MenuItem* selectButtonPress() override {
		if (currentSong->sessionClips.getIndexForClip(&clip()) < 0)
			display->displayPopup(l10n::get(l10n::String::STRING_FOR_TB3PO_SESSION_ONLY));
		else if (playbackHandler.recording != RecordingMode::OFF)
			display->displayPopup(l10n::get(l10n::String::STRING_FOR_RECORDING_IN_PROGRESS));
		else if (!clip().gridsFreezeReady())
			display->displayPopup(l10n::get(clip().gridsCaptureOverflowed()
			                                    ? l10n::String::STRING_FOR_GRIDS_CAPTURE_FULL
			                                    : l10n::String::STRING_FOR_TB3PO_WAIT_BAR));
		else if (!clip().gridsAvailable())
			display->displayPopup(l10n::get(l10n::String::STRING_FOR_GRIDS_UNSUPPORTED));
		else {
			const auto error = clip().freezeGridsBar(currentSong);
			if (error != Error::NONE)
				display->displayError(error);
			else {
				instrumentClipView.recalculateColours();
				uiNeedsRendering(getRootUI());
				display->displayPopup(l10n::get(l10n::String::STRING_FOR_TB3PO_FROZEN));
			}
		}
		return NO_NAVIGATION;
	}
};

} // namespace deluge::gui::menu_item::generator::grids
