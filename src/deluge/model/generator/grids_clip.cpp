#include "model/clip/instrument_clip.h"
#include "model/drum/drum.h"
#include "model/instrument/kit.h"
#include "model/instrument/melodic_instrument.h"
#include "model/note/note_row.h"
#include "model/song/song.h"
#include "playback/playback_handler.h"
#include <algorithm>

void InstrumentClip::initializeGridsMapping() {
	if (gridsMappingInitialized_ || !output || output->type != OutputType::KIT)
		return;
	for (uint8_t part = 0; part < 3; ++part) {
		gridsDrums_[part] = part < noteRows.getNumElements() ? noteRows.getElement(part)->drum : nullptr;
	}
	gridsMappingInitialized_ = true;
}
int32_t InstrumentClip::gridsRowIndex(uint8_t part) {
	if (part >= 3 || !gridsDrums_[part])
		return -1;
	for (int32_t i = 0; i < noteRows.getNumElements(); ++i) {
		if (noteRows.getElement(i)->drum == gridsDrums_[part])
			return i;
	}
	return -1;
}
bool InstrumentClip::gridsAvailable() {
	if (!output || (output->type != OutputType::KIT && output->type != OutputType::MIDI_OUT)
	    || arpSettings.mode != ArpMode::OFF || sequenceDirectionMode != SequenceDirection::FORWARD)
		return false;
	if (output->type == OutputType::KIT) {
		for (uint8_t part = 0; part < 3; ++part) {
			const int32_t index = gridsRowIndex(part);
			if (index < 0)
				continue;
			auto* drum = noteRows.getElement(index)->drum;
			if (drum->type != DrumType::SOUND || drum->arpSettings.mode != ArpMode::OFF)
				return false;
		}
	}
	return true;
}
deluge::model::generator::grids::Events
InstrumentClip::sendGridsEvents(ModelStackWithTimelineCounter* modelStack,
                                const deluge::model::generator::grids::Events& events) {
	deluge::model::generator::grids::Events emitted;
	if (!output || !isActiveOnOutput()) {
		gridsSounding_.fill(false);
		return emitted;
	}
	for (uint8_t i = 0; i < events.count; ++i) {
		const auto& event = events.notes[i];
		const auto part = event.part;
		if (!event.on && !gridsSounding_[part])
			continue;
		if (output->type == OutputType::KIT) {
			const int32_t index = gridsRowIndex(part);
			if (index < 0) {
				gridsSounding_[part] = false;
				continue;
			}
			auto* row = noteRows.getElement(index);
			auto* drum = row->drum;
			if (event.on && (row->muted || drum->type != DrumType::SOUND || drum->arpSettings.mode != ArpMode::OFF))
				continue;
			auto* rowStack = modelStack->addNoteRow(index, row);
			auto* soundStack = rowStack->addOtherTwoThings(drum->toModControllable(), &row->paramManager);
			if (event.on) {
				int16_t expression[kNumExpressionDimensions];
				row->getMPEValues(rowStack, expression);
				static_cast<Kit*>(output)->noteOnPreKitArp(
				    soundStack, drum, event.velocity, expression, MIDI_CHANNEL_NONE,
				    std::max<int32_t>(1, modelStack->song->getSixteenthNoteLength() / 4));
			}
			else {
				static_cast<Kit*>(output)->noteOffPreKitArp(soundStack, drum, event.velocity);
			}
		}
		else if (output->type == OutputType::MIDI_OUT) {
			auto* soundStack = modelStack->addOtherTwoThingsButNoNoteRow(output->toModControllable(), &paramManager);
			const int16_t expression[kNumExpressionDimensions]{};
			static_cast<MelodicInstrument*>(output)->sendNote(soundStack, event.on, gridsMidiNotes[part],
			                                                  event.on ? expression : nullptr, MIDI_CHANNEL_NONE,
			                                                  event.velocity);
		}
		else
			continue;
		gridsSounding_[part] = event.on;
		emitted.notes[emitted.count++] = event;
	}
	return emitted;
}
void InstrumentClip::stopGrids(ModelStackWithTimelineCounter* modelStack) {
	sendGridsEvents(modelStack, gridsRuntime_.stop());
	gridsHistory_.stop(playbackHandler.lastSwungTickActioned);
}
void InstrumentClip::forgetGridsDrum(ModelStackWithTimelineCounter* modelStack, Drum* drum) {
	for (auto& assigned : gridsDrums_) {
		if (assigned == drum) {
			stopGrids(modelStack);
			assigned = nullptr;
			gridsHistory_.reset();
		}
	}
}
bool InstrumentClip::setGridsDestination(ModelStackWithTimelineCounter* modelStack, uint8_t part, int32_t value) {
	if (part >= 3 || !output)
		return false;
	initializeGridsMapping();
	if (output->type == OutputType::KIT) {
		if (value < -1 || value >= noteRows.getNumElements())
			return false;
		Drum* drum = value < 0 ? nullptr : noteRows.getElement(value)->drum;
		for (uint8_t other = 0; other < 3; ++other)
			if (other != part && drum && gridsDrums_[other] == drum)
				return false;
		stopGrids(modelStack);
		gridsDrums_[part] = drum;
	}
	else if (output->type == OutputType::MIDI_OUT) {
		if (value < 0 || value > 127)
			return false;
		for (uint8_t other = 0; other < 3; ++other)
			if (other != part && gridsMidiNotes[other] == value)
				return false;
		stopGrids(modelStack);
		gridsMidiNotes[part] = value;
	}
	else
		return false;
	gridsHistory_.reset();
	expectEvent();
	return true;
}
void InstrumentClip::setGridsEnabled(ModelStackWithTimelineCounter* modelStack, bool enabled) {
	initializeGridsMapping();
	if (enabled == gridsEnabled || (enabled && !gridsAvailable()))
		return;
	if (enabled && isActiveOnOutput())
		stopAllNotesPlaying(modelStack);
	else
		stopGrids(modelStack);
	if (enabled)
		gridsHistory_.reset();
	gridsEnabled = enabled;
	refreshGrids();
	if (!enabled)
		setNoteRowPositions(modelStack, lastProcessedPos, lastProcessedPos);
	noteRowsNumTicksBehindClip = 0;
	expectEvent();
}
void InstrumentClip::processGrids(ModelStackWithTimelineCounter* modelStack) {
	int32_t ticksToNext = 1;
	if (!gridsAvailable() || !isActiveOnOutput())
		stopGrids(modelStack);
	else {
		const int64_t tick = static_cast<int64_t>(repeatCount) * loopLength + lastProcessedPos;
		const auto events =
		    gridsRuntime_.process(tick, std::max<int32_t>(1, modelStack->song->getSixteenthNoteLength() / 2));
		const auto emitted = sendGridsEvents(modelStack, events);
		const int32_t barLength = modelStack->song->getSixteenthNoteLength() * 16;
		if (output->type == OutputType::KIT) {
			deluge::model::generator::grids::Destinations destinations{};
			for (uint8_t part = 0; part < 3; ++part)
				destinations[part] = reinterpret_cast<uintptr_t>(gridsDrums_[part]);
			gridsHistory_.record(playbackHandler.lastSwungTickActioned, barLength, destinations, emitted);
		}
		ticksToNext =
		    std::min<int32_t>(events.ticksToNext, barLength - playbackHandler.lastSwungTickActioned % barLength);
	}
	ticksTilNextNoteRowEvent = ticksToNext;
	playbackHandler.swungTicksTilNextEvent = std::min(playbackHandler.swungTicksTilNextEvent, ticksToNext);
}
