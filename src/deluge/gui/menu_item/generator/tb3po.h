#pragma once

#include "gui/menu_item/integer.h"
#include "gui/menu_item/submenu.h"
#include "gui/menu_item/toggle.h"
#include "gui/ui_timer_manager.h"
#include "gui/views/instrument_clip_view.h"
#include "hid/display/oled.h"
#include "model/clip/instrument_clip.h"
#include "model/song/song.h"
#include "playback/playback_handler.h"
#include "util/functions.h"

namespace deluge::gui::menu_item::generator {

inline InstrumentClip& clip() {
	return *getCurrentInstrumentClip();
}

inline void edited() {
	clip().refreshGeneratorPattern(currentSong);
	clip().expectEvent();
}

class TB3POSubmenu final : public Submenu {
public:
	using Submenu::Submenu;
	std::string_view getTitle() const override {
		if (clip().generatorEnabled && !clip().generatorAvailable()) {
			return l10n::getView(l10n::String::STRING_FOR_TB3PO_PAUSED);
		}
		return l10n::getView(clip().generatorPending() ? l10n::String::STRING_FOR_TB3PO_PENDING : title);
	}
	void beginSession(MenuItem* from = nullptr) override {
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
	void readCurrentValue() override { setValue(clip().generatorEnabled); }
	void writeCurrentValue() override {
		char memory[MODEL_STACK_MAX_SIZE];
		auto* stack = currentSong->setupModelStackWithCurrentClip(memory);
		clip().setGeneratorEnabled(stack, getValue());
		readCurrentValue();
	}
	MenuItem* selectButtonPress() override {
		if (!clip().generatorEnabled && !clip().generatorAvailable()) {
			display->displayPopup(l10n::get(clip().arpSettings.mode != ArpMode::OFF
			                                    ? l10n::String::STRING_FOR_TB3PO_ARP_OFF
			                                    : l10n::String::STRING_FOR_TB3PO_FORWARD));
			return NO_NAVIGATION;
		}
		return Toggle::selectButtonPress();
	}
};

enum class Parameter { SEED, DENSITY, LENGTH, OCTAVE };

class Control final : public Integer {
public:
	Control(l10n::String name, Parameter parameter) : Integer(name), parameter_(parameter) {}
	void readCurrentValue() override {
		switch (parameter_) {
		case Parameter::SEED:
			setValue(clip().generatorSettings.seed);
			break;
		case Parameter::DENSITY:
			setValue(clip().generatorSettings.density);
			break;
		case Parameter::LENGTH:
			setValue(clip().generatorSettings.length);
			break;
		case Parameter::OCTAVE:
			setValue(clip().generatorOctave);
			break;
		}
	}
	void writeCurrentValue() override {
		switch (parameter_) {
		case Parameter::SEED:
			clip().generatorSettings.seed = getValue();
			break;
		case Parameter::DENSITY:
			clip().generatorSettings.density = getValue();
			break;
		case Parameter::LENGTH:
			clip().generatorSettings.length = getValue();
			break;
		case Parameter::OCTAVE:
			clip().generatorOctave = getValue();
			break;
		}
		edited();
	}
	void beginSession(MenuItem* from = nullptr) override {
		Integer::beginSession(from);
		uiTimerManager.setTimer(TimerName::UI_SPECIFIC, 200);
	}
	ActionResult timerCallback() override {
		renderUIsForOled();
		uiTimerManager.setTimer(TimerName::UI_SPECIFIC, 200);
		return ActionResult::DEALT_WITH;
	}

protected:
	int32_t getMinValue() const override {
		return parameter_ == Parameter::DENSITY ? -7 : (parameter_ == Parameter::LENGTH ? 1 : 0);
	}
	int32_t getMaxValue() const override {
		switch (parameter_) {
		case Parameter::SEED:
			return 9999; // Four digits on both OLED and 7-segment.
		case Parameter::DENSITY:
			return 7;
		case Parameter::LENGTH:
			return 32;
		case Parameter::OCTAVE:
			return 8;
		}
		return 0;
	}
	void drawPixelsForOled() override {
		Integer::drawPixelsForOled();
		if (clip().generatorPending()) {
			hid::display::OLED::main.drawStringCentred(l10n::get(l10n::String::STRING_FOR_TB3PO_NEXT_CYCLE),
			                                           OLED_MAIN_HEIGHT_PIXELS - 8, kTextSpacingX, kTextSizeYUpdated);
		}
	}

private:
	Parameter parameter_;
};

class Slides final : public Toggle {
public:
	using Toggle::Toggle;
	void readCurrentValue() override { setValue(clip().generatorSettings.slides); }
	void writeCurrentValue() override {
		clip().generatorSettings.slides = getValue();
		edited();
	}
};

class NewPattern final : public MenuItem {
public:
	using MenuItem::MenuItem;
	bool shouldEnterSubmenu() override { return false; }
	MenuItem* selectButtonPress() override {
		const uint32_t random = (getRandom255() << 8) | getRandom255();
		clip().generatorSettings.seed = (clip().generatorSettings.seed + 1 + random % 9999) % 10000;
		edited();
		char seed[12];
		intToString(clip().generatorSettings.seed, seed);
		display->displayPopup(seed);
		return NO_NAVIGATION;
	}
};

class Freeze final : public MenuItem {
public:
	using MenuItem::MenuItem;
	bool shouldEnterSubmenu() override { return false; }
	MenuItem* selectButtonPress() override {
		if (currentSong->sessionClips.getIndexForClip(&clip()) < 0) {
			display->displayPopup(l10n::get(l10n::String::STRING_FOR_TB3PO_SESSION_ONLY));
		}
		else if (playbackHandler.recording != RecordingMode::OFF) {
			display->displayPopup(l10n::get(l10n::String::STRING_FOR_RECORDING_IN_PROGRESS));
		}
		else if (!clip().generatorFreezeReady()) {
			display->displayPopup(l10n::get(l10n::String::STRING_FOR_TB3PO_WAIT_BAR));
		}
		else {
			Error error = clip().freezeGeneratorBar(currentSong);
			if (error != Error::NONE) {
				display->displayError(error);
			}
			else {
				// Freeze replaces NoteRows and can move yScroll. Refresh the pads beneath the open sound editor.
				instrumentClipView.recalculateColours();
				uiNeedsRendering(getRootUI());
				display->displayPopup(l10n::get(l10n::String::STRING_FOR_TB3PO_FROZEN));
			}
		}
		return NO_NAVIGATION;
	}
};

} // namespace deluge::gui::menu_item::generator
