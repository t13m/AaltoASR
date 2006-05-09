#ifndef HMMTRAINER_HH
#define HMMTRAINER_HH

#include "FeatureModules.hh"
#include "HmmSet.hh"
#include "Viterbi.hh"
#include "AdaReader.hh"
#include "Changeling.hh"
#include "TriphoneSet.hh"

// Matrix template library stuff
#include "mtl/mtl_config.h"
#include "mtl/mtl.h"
#include "mtl/matrix.h"
#include "mtl/lu.h"

#define TINY 1e-16

typedef mtl::matrix<float, mtl::rectangle<>, mtl::dense<>, 
		    mtl::row_major>::type Matrix;
typedef mtl::matrix<double, mtl::rectangle<>, mtl::dense<>, 
		    mtl::row_major>::type MatrixD;
typedef mtl::dense1D<float> Vector;
typedef mtl::dense1D<double> VectorD;
typedef mtl::external_vec<float> ExtVector;
typedef mtl::external_vec<const float> ExtVectorConst;


class HmmTrainer {
public:
  HmmTrainer();
  ~HmmTrainer();

  void init(HmmSet &model, FeatureGenerator &fea_gen, 
	    const char *adafile = NULL);
  void viterbi_train(int start_frame, int end_frame,
                     HmmSet &model,
                     FeatureGenerator &fea_gen, Viterbi &viterbi,
                     FILE *phn_out, std::string *speaker = NULL);
  void finish_train(HmmSet &model, FeatureGenerator &fea_gen,
		    const char *adafile = NULL);

  int num_unused_features(void) { return m_em_norm_warning_count; }
  double get_log_likelihood(void) { return m_log_likelihood; }

  void load_rule_set(const char *filename) { triphone_set.load_rule_set(filename); }
  void save_tying(const char *filename);
  
private:

  void print_line(FILE *f, int fr,
		  int start, int end, 
                  const std::string &label,
		  const std::string &speaker,
		  const std::string &comment);

  void update_parameters(HmmSet &model, HmmSet &model_tmp, 
                         const std::vector<float> &gk_norm);
  void update_transition_probabilities(HmmSet &model, HmmSet &model_tmp);
 
  // update_tmp_parameters returns the number of frames left untreated !
  int update_tmp_parameters(std::string *speaker,
			     HmmSet &model, HmmSet &model_tmp,
                             FeatureGenerator &fea_gen,
                             std::vector<float> &gk_norm, Viterbi &viterbi,
                             int start_frame, int end_frame);
  void update_morph_boundary_models(HmmSet &model, HmmSet &model_tmp,
                                    FeatureGenerator &fea_gen,
                                    std::vector<float> &gk_norm,
                                    Viterbi &viterbi,
                                    int start_frame, 
                                    int end_frame);
  void run_morph_boundary_update(HmmSet &model, HmmSet &model_tmp,
                                 FeatureGenerator &fea_gen,
                                 std::vector<float> &gk_norm,
                                 std::string hmm_label,
                                 int start_frame, 
                                 int end_frame);
  void update_state_kernels(HmmSet &model, HmmSet &model_tmp,
                            HmmState &state, HmmState &state_accu,
                            const FeatureVec &feature, int dim, bool update_ll,
                            std::vector<float> &gk_norm);
                                 
  void update_mllt_parameters(HmmSet &model, HmmSet &model_tmp,
                              FeatureGenerator &fea_gen,
                              std::vector<float> &gk_norm, Matrix &A);
  void update_hlda_parameters(HmmSet &model, HmmSet &model_tmp,
                              FeatureGenerator &fea_gen,
                              std::vector<float> &gk_norm, Matrix &A);
  void update_hlda_tmp_parameters(HmmSet &model, HmmSet &model_tmp,
                                  FeatureGenerator &fea_gen,
                                  std::vector<float> &gk_norm,
                                  Viterbi &viterbi, int start_frame, 
                                  int end_frame);
  void update_duration_statistics(HmmSet &model, Viterbi &viterbi, int frames);
  void update_boundary_duration_statistics(HmmSet &model,Viterbi *viterbi,
                                           int frames);
  void write_duration_statistics(HmmSet &model);

  void update_triphone_stat(FeatureGenerator &fea_gen,
                            Viterbi &viterbi,
                            int start_frame, 
                            int end_frame, HmmSet &model);

  void change_speaker(const std::string *new_speaker,
		      FeatureGenerator &fea_gen, HmmSet &model);
  void calculate_ada_tr(std::string &ada_speaker, int dim);

public:
  void set_info(int info) { m_info = info; }
  void set_transform_module(TransformationModule *mod) { m_transform_module = mod; }
  void set_mllt(bool mllt_flag) { m_mllt = mllt_flag; }
  void set_hlda(bool hlda_flag) { m_hlda = hlda_flag; }
  void set_min_var(float min_var) { m_min_var = min_var; }
  void set_win_size(int win_size) { m_win_size = win_size; }
  void set_overlap(float overlap) { m_overlap = overlap; }
  void set_cov_update(bool cov_update) { m_cov_update = cov_update; }
  void set_duration_statistics(bool durstat) { m_durstat = durstat; }
  void set_no_force_end(bool no_force_end) { m_no_force_end = no_force_end; }
  void set_print_segment(bool print_segment) {m_print_segment = print_segment;}
  void set_triphone_tying(bool tying) { m_triphone_tying = tying; }
  void set_fill_missing_contexts(bool fill) { m_fill_missing_contexts = fill; }
  void set_tying_min_count(int count) { m_tying_min_count = count; }
  void set_tying_min_likelihood_gain(double gain) { m_tying_min_lhg = gain; }
  void set_tying_length_award(float award) { m_tying_length_award = award; }
  void set_skip_short_silence_context(bool skip) { m_skip_short_silence_context = skip; }
  void set_ignore_length(bool il) { m_ignore_tying_length = il; }
  void set_print_speakered(bool sphn) { m_print_speakered = sphn; }
  void set_ordered_speakers(bool ordered_s) { m_ordered_s = ordered_s; }
  
private:

  int m_info;
  TransformationModule *m_transform_module;
  int m_source_dim;
  bool m_mllt;
  bool m_hlda;
  bool m_adap;
  float m_min_var;
  int m_win_size;
  float m_overlap;
  bool m_cov_update;
  bool m_durstat;
  bool m_triphone_tying;
  bool m_no_force_end;
  bool m_print_segment;
  double m_log_likelihood;

  bool m_fill_missing_contexts;
  int m_tying_min_count;
  double m_tying_min_lhg;
  double m_tying_length_award;
  bool m_ignore_tying_length;
  bool m_skip_short_silence_context;
  
  int **dur_table;
  int m_num_dur_models;

  bool m_print_speakered; // print speakered phns

  // MLLT stuff
  std::vector<float> gam;
  Matrix **cov_m;
  Matrix *m_transform_matrix;
  double mllt_determinant;
  
  // HLDA stuff
  std::vector<float> *kernel_means;
  std::vector<float> global_mean;
  Matrix *global_cov;
  int global_count;
  int m_em_norm_warning_count;

  // adaptation
  AdaReader m_ada; // reads adaptation matrices
  std::string m_speaker; // ID of current speaker
  std::vector<std::string> m_speakers; // speaker ID's
  std::map<std::string, Changeling*> m_changelings; // ada modules

  bool m_ordered_s; // files for a speaker are arranged successively

  HmmSet model_tmp;
  std::vector<float> gk_norm;

  std::string cur_tri_stat_left;
  std::string cur_tri_stat_center;
  std::string cur_tri_stat_right;
  int cur_tri_stat_state_index;
  int cur_tri_stat_state;
  int cur_tri_stat_hmm_index;
  TriphoneSet triphone_set;
};


#endif // HMMTRAINER_HH