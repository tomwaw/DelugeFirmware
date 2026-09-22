#include "gui/ui/ui.h"
#include "memory/general_memory_allocator.h"
#include "model/action/action_logger.h"
#include "model/clip/instrument_clip.h"
#include "model/consequence/consequence.h"
#include "model/note/note.h"
#include "model/note/note_row.h"
#include "model/output.h"
#include "model/song/song.h"
#include "playback/playback_handler.h"
#include <new>
#include <utility>

namespace {

// Own whichever complete NoteRow set is not currently in the clip. Undo and redo are allocation-free swaps.
class ConsequenceTB3POFreeze final : public Consequence {
public:
	ConsequenceTB3POFreeze(InstrumentClip& clip, int32_t barLength)
	    : clip_(clip), length_(barLength), originalLength_(barLength), settings_(clip.generatorSettings),
	      octave_(clip.generatorOctave) {}

	Error build(const deluge::model::generator::tb3po::CapturedBar& bar) {
		for (uint8_t i = 0; i < bar.count; ++i) {
			const auto& captured = bar.notes[i];
			int32_t rowIndex = rows_.search(captured.note, GREATER_OR_EQUAL);
			NoteRow* row = rowIndex < rows_.getNumElements() ? rows_.getElement(rowIndex) : nullptr;
			if (!row || row->y != captured.note) {
				row = rows_.insertNoteRowAtY(captured.note);
			}
			if (!row) {
				return Error::INSUFFICIENT_RAM;
			}
			int32_t index = row->notes.insertAtKey(captured.pos);
			if (index < 0) {
				return Error::INSUFFICIENT_RAM;
			}
			auto* note = row->notes.getElement(index);
			note->pos = captured.pos;
			note->length = deluge::model::generator::tb3po::frozenNoteLength(bar, i);
			note->velocity = captured.velocity;
			note->lift = kDefaultLiftValue;
			note->probability = kNumProbabilityValues;
			note->iterance = kDefaultIteranceValue;
			note->fill = static_cast<uint8_t>(FillMode::OFF);
		}
		return Error::NONE;
	}

	Error revert(TimeType, ModelStack* modelStack) override {
		auto* stack = modelStack->addTimelineCounter(&clip_);
		clip_.stopAllNotesPlaying(stack);
		clip_.noteRows.swapStateWith(&rows_);
		std::swap(clip_.loopLength, length_);
		std::swap(clip_.originalLength, originalLength_);
		std::swap(clip_.generatorEnabled, enabled_);
		std::swap(clip_.generatorSettings, settings_);
		std::swap(clip_.generatorOctave, octave_);
		std::swap(clip_.sequenceDirectionMode, direction_);
		std::swap(clip_.arpSettings.mode, arpMode_);
		clip_.refreshGeneratorPattern(modelStack->song);
		// Continue at the transport's position within the new loop. Clip/output identity and launch state stay intact.
		const bool running = playbackHandler.isEitherClockActive();
		const int32_t pos = running ? playbackHandler.lastSwungTickActioned % clip_.loopLength : 0;
		clip_.setPos(stack, pos, false);
		if (running && clip_.isActiveOnOutput() && modelStack->song->isClipActive(&clip_)) {
			clip_.resumePlayback(stack, true);
		}
		return Error::NONE;
	}

private:
	InstrumentClip& clip_;
	NoteRowVector rows_;
	int32_t length_;
	int32_t originalLength_;
	bool enabled_ = false;
	deluge::model::generator::tb3po::Settings settings_;
	int32_t octave_;
	SequenceDirection direction_ = SequenceDirection::FORWARD;
	ArpMode arpMode_ = ArpMode::OFF;
};

} // namespace

Error InstrumentClip::freezeGeneratorBar(Song* song) {
	if (!generatorFreezeReady() || song->sessionClips.getIndexForClip(this) < 0 || !output
	    || output->type != OutputType::SYNTH || playbackHandler.recording != RecordingMode::OFF) {
		return Error::UNSPECIFIED;
	}
	// Snapshot before allocations can yield to audio. Publish only after both notes and undo storage are ready.
	const auto bar = generatorHistory_.completed();
	void* memory = GeneralMemoryAllocator::get().allocLowSpeed(sizeof(ConsequenceTB3POFreeze));
	if (!memory) {
		return Error::INSUFFICIENT_RAM;
	}
	auto* change = new (memory) ConsequenceTB3POFreeze(*this, bar.length);
	Error error = change->build(bar);
	if (error != Error::NONE) {
		change->~ConsequenceTB3POFreeze();
		delugeDealloc(change);
		return error;
	}
	Action* action = actionLogger.getNewAction(ActionType::TB3PO_FREEZE);
	if (!action) {
		change->~ConsequenceTB3POFreeze();
		delugeDealloc(change);
		return Error::INSUFFICIENT_RAM;
	}
	// Undo returns to the underlying clip view, not to a sound-editor menu promoted into a root UI.
	action->view = getRootUI();
	action->addConsequence(change);
	char stackMemory[MODEL_STACK_MAX_SIZE];
	change->revert(AFTER, setupModelStackWithSong(stackMemory, song));
	if (noteRows.getNumElements()) {
		yScroll = song->getYVisualFromYNote(noteRows.getElement(0)->y, inScaleMode);
	}
	actionLogger.updateAction(action);
	actionLogger.closeAction(ActionType::TB3PO_FREEZE);
	return Error::NONE;
}
