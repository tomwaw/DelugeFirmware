#include "gui/ui/ui.h"
#include "memory/general_memory_allocator.h"
#include "model/action/action_logger.h"
#include "model/clip/instrument_clip.h"
#include "model/consequence/consequence.h"
#include "model/note/note.h"
#include "model/note/note_row.h"
#include "model/song/song.h"
#include "playback/playback_handler.h"
#include <new>
#include <utility>

// Swap only notes and timing. The real kit rows retain their drum, sound/automation managers and mute state.
class ConsequenceGridsFreeze final : public Consequence {
	struct RowState {
		RowState* next = nullptr;
		Drum* drum = nullptr;
		int32_t index = 0;
		NoteVector notes;
		int32_t length = 0;
		SequenceDirection direction = SequenceDirection::OBEY_PARENT;
	};

public:
	ConsequenceGridsFreeze(InstrumentClip& clip, int32_t length)
	    : clip_(clip), length_(length), originalLength_(length), settings_(clip.gridsSettings),
	      drums_(clip.gridsDrums_), midiNotes_(clip.gridsMidiNotes),
	      mappingInitialized_(clip.gridsMappingInitialized_) {}
	~ConsequenceGridsFreeze() override {
		while (rows_) {
			auto* row = rows_;
			rows_ = row->next;
			row->~RowState();
			delugeDealloc(row);
		}
	}
	Error build(const deluge::model::generator::grids::CapturedBar& bar) {
		for (int32_t index = 0; index < clip_.noteRows.getNumElements(); ++index) {
			auto* source = clip_.noteRows.getElement(index);
			void* memory = GeneralMemoryAllocator::get().allocLowSpeed(sizeof(RowState));
			if (!memory)
				return Error::INSUFFICIENT_RAM;
			auto* row = new (memory) RowState;
			row->drum = source->drum;
			row->index = index;
			row->next = rows_;
			rows_ = row;
			for (uint8_t i = 0; i < bar.count; ++i) {
				const auto& captured = bar.notes[i];
				if (reinterpret_cast<uintptr_t>(row->drum) != bar.destinations[captured.part])
					continue;
				int32_t noteIndex = row->notes.insertAtKey(captured.pos);
				if (noteIndex < 0)
					return Error::INSUFFICIENT_RAM;
				auto* note = row->notes.getElement(noteIndex);
				note->pos = captured.pos;
				note->length = captured.length;
				note->velocity = captured.velocity;
				note->lift = kDefaultLiftValue;
				note->probability = kNumProbabilityValues;
				note->iterance = kDefaultIteranceValue;
				note->fill = static_cast<uint8_t>(FillMode::OFF);
			}
		}
		return Error::NONE;
	}
	Error revert(TimeType, ModelStack* modelStack) override {
		// Validate every destination before any swaps; subsequent ordinary edits should have been undone first.
		for (auto* saved = rows_; saved; saved = saved->next)
			if (!resolve(*saved))
				return Error::BUG;
		auto* stack = modelStack->addTimelineCounter(&clip_);
		clip_.stopAllNotesPlaying(stack);
		for (auto* saved = rows_; saved; saved = saved->next) {
			auto* row = resolve(*saved);
			row->notes.swapStateWith(&saved->notes);
			std::swap(row->loopLengthIfIndependent, saved->length);
			std::swap(row->sequenceDirectionMode, saved->direction);
		}
		std::swap(clip_.loopLength, length_);
		std::swap(clip_.originalLength, originalLength_);
		std::swap(clip_.gridsEnabled, enabled_);
		std::swap(clip_.gridsSettings, settings_);
		std::swap(clip_.gridsDrums_, drums_);
		std::swap(clip_.gridsMidiNotes, midiNotes_);
		std::swap(clip_.gridsMappingInitialized_, mappingInitialized_);
		std::swap(clip_.sequenceDirectionMode, direction_);
		std::swap(clip_.arpSettings.mode, arpMode_);
		clip_.refreshGrids();
		const bool running = playbackHandler.isEitherClockActive();
		clip_.setPos(stack, running ? playbackHandler.lastSwungTickActioned % clip_.loopLength : 0, false);
		if (running && clip_.isActiveOnOutput() && modelStack->song->isClipActive(&clip_))
			clip_.resumePlayback(stack, true);
		return Error::NONE;
	}

private:
	NoteRow* resolve(const RowState& saved) {
		if (saved.drum)
			return clip_.getNoteRowForDrum(saved.drum);
		if (saved.index >= clip_.noteRows.getNumElements())
			return nullptr;
		auto* row = clip_.noteRows.getElement(saved.index);
		return row->drum ? nullptr : row;
	}
	InstrumentClip& clip_;
	RowState* rows_ = nullptr;
	int32_t length_, originalLength_;
	bool enabled_ = false;
	deluge::model::generator::grids::Settings settings_;
	std::array<Drum*, 3> drums_;
	std::array<uint8_t, 3> midiNotes_;
	bool mappingInitialized_;
	SequenceDirection direction_ = SequenceDirection::FORWARD;
	ArpMode arpMode_ = ArpMode::OFF;
};

Error InstrumentClip::freezeGridsBar(Song* song) {
	if (!gridsFreezeReady() || !output || output->type != OutputType::KIT || !gridsAvailable()
	    || song->sessionClips.getIndexForClip(this) < 0 || playbackHandler.recording != RecordingMode::OFF)
		return Error::UNSPECIFIED;
	// Capture by value before allocation can yield to playback and advance the history.
	const auto bar = gridsHistory_.completed();
	for (uint8_t part = 0; part < 3; ++part) {
		if (bar.destinations[part] != reinterpret_cast<uintptr_t>(gridsDrums_[part])
		    || (bar.destinations[part] && gridsRowIndex(part) < 0))
			return Error::UNSPECIFIED;
	}
	void* memory = GeneralMemoryAllocator::get().allocLowSpeed(sizeof(ConsequenceGridsFreeze));
	if (!memory)
		return Error::INSUFFICIENT_RAM;
	auto* change = new (memory) ConsequenceGridsFreeze(*this, bar.length);
	Error error = change->build(bar);
	if (error != Error::NONE) {
		change->~ConsequenceGridsFreeze();
		delugeDealloc(change);
		return error;
	}
	Action* action = actionLogger.getNewAction(ActionType::GRIDS_FREEZE);
	if (!action) {
		change->~ConsequenceGridsFreeze();
		delugeDealloc(change);
		return Error::INSUFFICIENT_RAM;
	}
	action->view = getRootUI();
	action->addConsequence(change);
	char stackMemory[MODEL_STACK_MAX_SIZE];
	error = change->revert(AFTER, setupModelStackWithSong(stackMemory, song));
	actionLogger.updateAction(action);
	actionLogger.closeAction(ActionType::GRIDS_FREEZE);
	return error;
}
