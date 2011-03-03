#ifndef TOOLBOX_HH
#define TOOLBOX_HH

#include <deque>

#include "io.hh"
#include "WordGraph.hh"
#include "NowayHmmReader.hh"
#include "NowayLexiconReader.hh"
#include "TPNowayLexReader.hh"
#include "LnaReaderCircular.hh"
#include "Expander.hh"
#include "Search.hh"
#include "TokenPassSearch.hh"
#include "OneFrameAcoustics.hh"

class Toolbox {
public:
  Toolbox();
  ~Toolbox();

  // Decoder selection: 0=token pass, 1 = stack
  void select_decoder(int stack_dec) { m_use_stack_decoder = stack_dec; }
  
  // HMM models
  void hmm_read(const char *file);
  void duration_read(const char *dur_file);
  const std::vector<Hmm> &hmms() const { return m_hmms; }

  // Lexicon

  /// \brief Reads a dictionary file that specifies the lexicon and
  /// pronunciations used in the decoding.
  ///
  /// \param file Name of a NOWAY dictionary file where the lexicon is
  /// read from.
  ///
  void lex_read(const char *file);

  const std::string &lex_word() const { return m_lexicon_reader.word(); }
  const std::string &lex_phone() const { return m_lexicon_reader.phone(); }
  const std::string &word(int index) const 
  {
    if (m_use_stack_decoder)
      return m_vocabulary.word(index); 
    return m_tp_vocabulary.word(index);
  }

  // Ngram
  void ngram_read(const char *file, float weight, bool binary=true);
  void read_lookahead_ngram(const char *file, bool binary=true);

  /// \brief Reads a finite-state automaton language model.
  ///
  /// \param file Name of the file where the language model is read from.
  /// \param binary If set to true, the file is expected to be in a non-standard
  /// format that lm utility writes. Otherwise ARPA format is expected.
  ///
  void fsa_lm_read(const char *file, bool binary=true);

  // Lna
  void lna_open(const char *file, int size);
  void lna_close();
  void lna_seek(int frame) { m_lna_reader.seek(frame); }
  Acoustics &acoustics() { return *m_acoustics; }
  void use_one_frame_acoustics() 
  { 
    m_acoustics = &m_one_frame_acoustics; 
    m_tp_search.set_acoustics(m_acoustics);
  }
  void set_one_frame(int frame, const std::vector<float> log_probs)
  {
    assert(m_acoustics == &m_one_frame_acoustics);
    m_one_frame_acoustics.set(frame, log_probs);
  }

  // Expander
  void expand(int frame, int frames);
  const std::string &best_word();
  void print_words(int words);
  int find_word(const std::string &word);
  std::vector<Expander::Word*> words() { return m_expander.words(); }

  // Stack search
  void init(int expand_window);
  bool expand_stack(int frame) { return m_search.expand_stack(frame); }
  void expand_words(int frame, const std::string &words) 
  { m_search.expand_words(frame, words); }
  void go(int frame) { m_search.go(frame); }
  bool runto(int frame);
  bool recognize_segment(int start_frame, int end_frame);

  // Both searches
  void reset(int frame) { if (m_use_stack_decoder) m_search.reset_search(frame);  else m_tp_search.reset_search(frame);}
  void set_end(int frame) { if (m_use_stack_decoder) m_search.set_end_frame(frame); else m_tp_search.set_end_frame(frame); }

  /// \brief Proceeds decoding one frame.
  ///
  /// \return true if a frame was available, false if there are no more frames.
  ///
  bool run() { return (m_use_stack_decoder?m_search.run():m_tp_search.run()); }

  // Token pass search
  WordGraph &tp_word_graph() { return m_tp_search.word_graph; } 
  void write_word_graph(const std::string &file_name)
  { m_tp_search.write_word_graph(file_name); }
  void print_best_lm_history(FILE *out=stdout) 
  { 
    m_tp_search.print_lm_history(out, true); 
  }
  void print_best_lm_history_to_file(FILE *out) {print_best_lm_history(out);}

  // Miscellaneous
  void segment(const std::string &str, int start_frame, int end_frame);

  // Info
  TokenPassSearch &tp_search() { return m_tp_search; }
  int frame() { return (m_use_stack_decoder?m_search.frame():m_tp_search.frame()); }
  int first_frame() { return m_search.first_frame(); }
  int last_frame() { return m_search.last_frame(); }
  HypoStack &stack(int frame) { return m_search.stack(frame); }
  void prune(int frame, int top);
  int paths() const { return HypoPath::g_count; }

  // Options
  void set_forced_end(bool forced_end) 
  { m_expander.set_forced_end(forced_end); }
  void set_hypo_limit(int hypo_limit) { m_search.set_hypo_limit(hypo_limit); } 
  void set_prune_similar(int prune_similar) { m_search.set_prune_similar(prune_similar); m_tp_search.set_similar_lm_history_span(prune_similar); } 
  void set_word_limit(int word_limit) { m_search.set_word_limit(word_limit); }
  void set_word_beam(float word_beam) { m_search.set_word_beam(word_beam); }
  void set_lm_scale(float lm_scale) { m_search.set_lm_scale(lm_scale); m_tp_search.set_lm_scale(lm_scale); }
  void set_lm_offset(float lm_offset) { m_search.set_lm_offset(lm_offset); }
  void set_unk_offset(float unk_offset) { m_search.set_unk_offset(unk_offset); }
  void set_token_limit(int limit) { m_expander.set_token_limit(limit); m_tp_search.set_max_num_tokens(limit); }
  void set_state_beam(float beam) { m_expander.set_beam(beam); }
  void set_duration_scale(float scale) { m_expander.set_duration_scale(scale); m_tp_search.set_duration_scale(scale); }
  void set_transition_scale(float scale) { m_expander.set_transition_scale(scale); m_tp_search.set_transition_scale(scale); }
  void set_rabiner_post_mode(int mode) { m_expander.set_rabiner_post_mode(mode); }
  void set_hypo_beam(float beam) { m_search.set_hypo_beam(beam); }
  void set_global_beam(float beam) 
  { m_search.set_global_beam(beam); m_tp_search.set_global_beam(beam); }
  void set_word_end_beam(float beam) { m_tp_search.set_word_end_beam(beam); }
  void set_eq_depth_beam(float beam) { m_tp_search.set_eq_depth_beam(beam); }
  void set_eq_word_count_beam(float beam) { m_tp_search.set_eq_word_count_beam(beam); }
  void set_fan_in_beam(float beam) { m_tp_search.set_fan_in_beam(beam); }
  void set_fan_out_beam(float beam) { m_tp_search.set_fan_out_beam(beam); }
  void set_tp_state_beam(float beam) { m_tp_search.set_state_beam(beam); }
  void set_max_state_duration(int duration) 
  { m_expander.set_max_state_duration(duration); }
  void set_lm_lookahead(int lmlh) { m_tp_lexicon.set_lm_lookahead(lmlh); m_tp_search.set_lm_lookahead(lmlh); }
  void set_cross_word_triphones(bool cw_triphones) { m_tp_lexicon.set_cross_word_triphones(cw_triphones); }
  void set_insertion_penalty(float ip) { m_tp_search.set_insertion_penalty(ip); }
  void set_silence_is_word(bool b) { m_tp_lexicon.set_silence_is_word(b); m_tp_lexicon_reader.set_silence_is_word(b); }
  void set_ignore_case(bool b) { m_tp_lexicon.set_ignore_case(b);}
  void set_verbose(int verbose) { m_search.set_verbose(verbose); m_tp_lexicon.set_verbose(verbose); m_tp_search.set_verbose(verbose);}
  void set_print_text_result(int print) { m_tp_search.set_print_text_result(print); }
  void set_print_state_segmentation(int print) { m_tp_search.set_print_state_segmentation(print); }
  void set_keep_state_segmentation(int value) { m_tp_search.set_keep_state_segmentation(value); }
  void set_print_probs(bool print_probs) 
  { 
    m_search.set_print_probs(print_probs); 
    m_tp_search.set_print_probs(print_probs);
  }
  void set_multiple_endings(int multiple_endings) 
  { m_search.set_multiple_endings(multiple_endings); }
  void set_print_indices(bool print_indices) 
  { m_search.set_print_indices(print_indices); }
  void set_print_frames(bool print_frames) 
  { 
    m_search.set_print_frames(print_frames); 
  }
  void set_word_boundary(const std::string &word)
  { if (m_use_stack_decoder) m_search.set_word_boundary(word); else m_word_boundary = word; }
  void set_sentence_boundary(const std::string &start, const std::string &end) { m_tp_search.set_sentence_boundary(start, end); }
  void set_dummy_word_boundaries(bool value)
  { m_search.set_dummy_word_boundaries(value); }
  void set_require_sentence_end(bool s) { m_tp_search.set_require_sentence_end(s); }

  void set_optional_short_silence(bool state) { m_tp_lexicon.set_optional_short_silence(state); }

  void prune_lm_lookahead_buffers(int min_delta, int max_depth) { m_tp_lexicon.prune_lookahead_buffers(min_delta, max_depth); }
  void set_generate_word_graph(bool value)
  { m_tp_search.set_generate_word_graph(value); }
  void set_use_lm_cache(bool value)
  { m_tp_search.set_use_lm_cache(value); }

  // Debug
  void print_prunings()
  { m_search.print_prunings(); }
  void print_hypo(Hypo &hypo);
  void print_sure() { m_search.print_sure(); }
  void write_word_history(const std::string file_name) {
    io::Stream out(file_name, "w");
    m_tp_search.write_word_history(out.file);
  }
  void write_word_history() { m_tp_search.write_word_history(); }
  void print_lm_history() { m_tp_search.print_lm_history(); }
  void write_state_segmentation(const std::string &file)
  { 
    m_tp_search.print_state_history(io::Stream(file, "w").file);
  }

  TokenPassSearch &debug_get_tp() { return m_tp_search; }
  TPLexPrefixTree &debug_get_tp_lex() { return m_tp_lexicon; }
  void debug_print_best_lm_history() 
  { m_tp_search.debug_print_best_lm_history(); }

  struct OpenError : public std::exception {
    virtual const char *what() const throw()
    { return "Toolbox: open error"; }
  };

  void print_tp_lex_node(int node) { m_tp_lexicon.print_node_info(node); }
  void print_tp_lex_lookahead(int node) {m_tp_lexicon.print_lookahead_info(node, m_tp_vocabulary); }

private:
  int m_use_stack_decoder;
  
  NowayHmmReader m_hmm_reader;
  std::map<std::string,int> &m_hmm_map;
  std::vector<Hmm> &m_hmms;

  NowayLexiconReader m_lexicon_reader;
  Lexicon &m_lexicon;
  const Vocabulary &m_vocabulary;

  TPLexPrefixTree m_tp_lexicon;
  TPNowayLexReader m_tp_lexicon_reader;
  Vocabulary m_tp_vocabulary;
  TokenPassSearch m_tp_search;
  
  Acoustics *m_acoustics;
  LnaReaderCircular m_lna_reader;
  OneFrameAcoustics m_one_frame_acoustics;

  std::string m_word_boundary;

  std::vector<TreeGram*> m_ngrams;
  fsalm::LM *m_fsa_lm;
  std::deque<int> m_history;
  TreeGram *m_lookahead_ngram;

  Expander m_expander;

  Search m_search;
};

#endif /* TOOLBOX_HH */
