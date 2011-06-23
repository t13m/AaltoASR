#!/usr/bin/python
#
# Recognizes test commands for cariological status dictation. Takes the
# training data directory as an argument, e.g.
#   ./cariology_test.py ~/DentistryData

import time
import string
import sys
import os
import subprocess
import re
import tempfile
import filecmp
import gzip
import math

# Set your decoder swig path in here!
sys.path.append(os.path.dirname(sys.argv[0]) + "/src/swig");

import Decoder

##################################################
# Initialize
#

akupath = "../aku"

# 16 kHz acoustic model
#ac_model = sys.argv[1] + "/test_mfcc_noisy_trained";
#speech_directory = sys.argv[1] + "/speech/Sennheiser_16kHz"
# 8 kHz (telephone?) acoustic model
ac_model = sys.argv[1] + "/speechdat_gain5000_occ300_23.2.2009_22";
speech_directory = sys.argv[2]

hmms = ac_model + ".ph"
dur = ac_model + ".dur"

# language model for cariology status commands
#lexicon = sys.argv[1] + "/CariologyLexicon.lex"
#ngram = sys.argv[1] + "/CariologyLM.even.3gram.bin"
#lookahead_ngram = sys.argv[1] + "/CariologyLM.even.2gram.bin"

lexicon = "/share/work/jpylkkon/bin_lm/morph19k.lex"
ngram = "/share/work/jpylkkon/bin_lm/morph19k_D20E10_varigram.bin"
lookahead_ngram = "/share/work/jpylkkon/bin_lm/morph19k_2gram.bin"


lm_scale = 30
global_beam = 400


##################################################
# Generate LNA files for recognition
#

recipe_file = tempfile.NamedTemporaryFile()

for wav_file in os.listdir(speech_directory):
	if not wav_file.endswith('.wav'):
		continue;
	audio = speech_directory + "/" + wav_file
	lna = wav_file[:-4] + ".lna"
	if not os.path.exists(speech_directory + "/" + lna):
		recipe_file.write("audio=" + audio + " lna=" + lna + "\n")

# Don't close the temporary file yet, or it will get deleted.
recipe_file.flush()

print "Computing phoneme probabilities."
command = [akupath + "/phone_probs", \
		"-b", ac_model, \
 		"-c", ac_model + ".cfg", \
 		"-r", recipe_file.name, \
		"-o", speech_directory]
#command = [akupath + "/phone_probs", \
#		"-b", ac_model, \
#		"-c", ac_model + ".cfg", \
#		"-r", recipe_file.name, \
#		"-C", ac_model + ".gcl", \
#		"-o", speech_directory, \
#		"--eval-ming", "0.50"]
try:
	result = subprocess.check_call(command)
except subprocess.CalledProcessError as e:
	print "phone_probs returned a non-zero exit status."
	sys.exit(-1)

recipe_file.close()


##################################################
# Recognize
#

t = Decoder.Toolbox()

t.select_decoder(0)

# Emable for morph models.
t.set_silence_is_word(1)

t.set_optional_short_silence(1)

t.set_cross_word_triphones(1)
# t.set_require_sentence_end(0)
t.set_require_sentence_end(1)

print "Loading acoustic model."
t.hmm_read(hmms)
t.duration_read(dur)

t.set_verbose(1)
t.set_print_text_result(0)

# Generate a lattice of the word sequences that the decoder considered. Requires
# 2-grams or higher order model.
t.set_generate_word_graph(1)

t.set_lm_lookahead(1)

# Emable for morph models.
t.set_word_boundary("<w>")

print "Loading lexicon."
try:
    t.lex_read(lexicon)
except:
    print "phone:", t.lex_phone()
    sys.exit(-1)
t.set_sentence_boundary("<s>", "</s>")

print "Loading language model."
t.ngram_read(ngram, 1)
# t.fsa_lm_read(ngram, 1)
t.read_lookahead_ngram(lookahead_ngram)

t.prune_lm_lookahead_buffers(0, 4) # min_delta, max_depth

word_end_beam = int(2 * global_beam / 3);
trans_scale = 1
dur_scale = 3

t.set_global_beam(global_beam)
t.set_word_end_beam(word_end_beam)
t.set_token_limit(30000)

# Should equal to the n-gram model order.
t.set_prune_similar(3)

t.set_print_probs(0)
t.set_print_indices(0)
t.set_print_frames(0)

t.set_duration_scale(dur_scale)
t.set_transition_scale(trans_scale)
t.set_lm_scale(lm_scale)
# t.set_insertion_penalty(-0.5)

print "Recognizing audio files."
for lna_file in os.listdir(speech_directory):
	if not lna_file.endswith('.lna'):
		continue
	print ":: " + lna_file[:-4]
	lna_path = speech_directory + "/" + lna_file
	rec_path = lna_path[:-4] + ".rec"
	txt_path = lna_path[:-4] + ".txt"
	slf_path = lna_path[:-4] + ".slf"
	t.lna_open(lna_path, 1024)
	t.reset(0)
	t.set_end(-1)
	while (True):
		if (not t.run()):
			# We have to open with only "w" first, and then later with "r"
			# for reading, or the file will not be written.
			rec = open(rec_path, "w")
			t.print_best_lm_history_to_file(rec)
			t.write_word_graph(slf_path);
			rec.close()
			
			command = 'lattice-tool -read-htk -in-lattice "' + \
					slf_path + \
					'" -nbest-decode 100 -out-nbest-dir "' + \
					speech_directory + '"'
			return_value = os.system(command)
			if return_value != 0:
				print "Command returned a non-zero exit status: ", command
				sys.exit(1)
    		
			nbest_file = gzip.open(slf_path + ".gz", "rb")
			nbest_list = nbest_file.readlines()
			nbest_file.close()

			rec = open(rec_path, "r")
			recognition = rec.read().strip()
			rec.close()
			break

	if os.path.exists(txt_path):
		equal = filecmp.cmp(rec_path, txt_path)
		if equal:
			print "OK ", recognition
		else:
			txt = open(txt_path, "r")
			transcription = txt.read().strip()
			txt.close()
			print "F ", recognition, " != ", transcription
	else:
		print "? ", recognition

	# Compensate for incorrect assumptions in the HMM by flattening the
	# logprobs.
	alpha = 0.1

	line = nbest_list[0]
	logprob_1 = float(line.split(' ')[0]) * alpha
	
	total_logprob = logprob_1
	for line in nbest_list[1:]:
		logprob = float(line.split(' ')[0]) * alpha
		# Acoustic probabilities are calculated in natural logarithm space.
		total_logprob += math.log(1 + math.exp(logprob - total_logprob))
	
	print str(logprob_1 - total_logprob)
