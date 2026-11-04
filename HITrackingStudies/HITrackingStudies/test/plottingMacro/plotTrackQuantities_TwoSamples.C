///Compare track-level quantities between two samples that contain the SAME events
///(e.g. the same GEN-SIM reconstructed with two releases):
///  top pad    : distribution (log y), sample1 = blue open circles, sample2 = red open squares
///  bottom pad : ratio sample1/sample2 with CORRELATED (event-paired) uncertainties, dashed line at 1
///Quantities: pT, eta, phi, number of hits, chi2/ndf/nlayers, algo, pTErr/pT, dz/sigma_dz, dxy/sigma_dxy
///
///Ratio uncertainty: events are matched between the two files by (run, lumi, event). For each bin,
///R = S1/S2 with S = sum over events of the per-event counts, and
///  Var(R) = R^2 * N/(N-1) * sum_e z_e^2,  z_e = n1_e/S1 - n2_e/S2 - (d1_e/D1 - d2_e/D2)
///(delta method with events as independent units; d/D is the normalization term, zero for "none").
///Because both samples see the same events, the event-to-event fluctuations cancel and only genuine
///reconstruction differences remain. The uncorrelated (independent-sample) error is printed for comparison.
///
///Input: files with the anaTrack tree ("anaTrack/trackTree"), e.g. from
///  cmsRun run_PbPb_cfg.py sample=MC_RecoDebug saveTree=True ...
///
///Run:
///  root -l -b -q 'plotTrackQuantities_TwoSamples.C("trk_rereco.root","trk_prompt.root","ReReco","Prompt",0,10)'
///Arguments: file1, file2, label1, label2, cent_min, cent_max (in %), [output directory], [normalization]
///Normalization: "none"  = raw number of tracks per bin (default; use when both samples have the same events)
///               "event" = tracks per event per bin
///               "area"  = unit area per unit x (shape-only comparison)
///e.g. root -l -b -q 'plotTrackQuantities_TwoSamples.C("a.root","b.root","A","B",0,10,"","area")'
///Output: one PDF+PNG per quantity, a 3x3 summary (TrackQuantities_summary.pdf/png) and all histograms in a ROOT file.

#include "TFile.h"
#include "TTree.h"
#include "TTreeReader.h"
#include "TTreeReaderValue.h"
#include "TTreeReaderArray.h"
#include "TH1D.h"
#include "TAxis.h"
#include "TCanvas.h"
#include "TPad.h"
#include "TLegend.h"
#include "TLatex.h"
#include "TLine.h"
#include "TStyle.h"
#include "TSystem.h"
#include "TMath.h"
#include <iostream>
#include <vector>
#include <array>
#include <map>
#include <set>
#include <algorithm>
#include <cmath>

///*** Important parameters
const char* treeName = "anaTrack/trackTree";
bool  hpOnly = true;      // use only highPurity tracks (as in the reference plots)
float ptMin  = 0.0;       // pT range of the selected tracks (GeV)
float ptMax  = 99999.;
double ratioMin = 0.0;    // y-range of the ratio panel; zoom in (e.g. 0.9-1.1) to see small differences
double ratioMax = 2.0;

struct TrkVar {
  const char* name;     // histogram / file name
  const char* xtitle;
  int nbins; double xmin, xmax;
};

enum { kPt, kEta, kPhi, kNHit, kChi2, kAlgo, kPtRes, kDzSig, kDxySig, kNVars };

const std::vector<TrkVar> trkVars = {
  {"pt",     "p_{T} (GeV)",           100,   0.0,  10.0},
  {"eta",    "#eta",                   60,  -3.0,   3.0},
  {"phi",    "#phi",                   64, -TMath::Pi(), TMath::Pi()},
  {"nhits",  "Number Of Hits",         50,   0.0,  50.0},
  {"chi2",   "#chi^{2}/ndf/nlayers",  100,   0.0,   1.0},
  {"algo",   "Algo",                   30,   0.0,  30.0},
  {"ptres",  "p_{T} Err/p_{T}",       100,   0.0,   0.3},
  {"dzsig",  "dZ/#sigma_{dZ}",        100, -30.0,  30.0},
  {"dxysig", "dXY/#sigma_{dXY}",      100, -30.0,  30.0},
};

TString yAxisTitle = "Number of tracks"; // set from the normalization option

typedef std::array<int,3> EvtKey; // run, lumi, event

//Per-event content of one sample: counts in every (variable, bin), centrality pass flag
struct EvtData {
  bool passCent = false;
  std::vector<float> counts;   // concatenated over variables, nbins+2 per variable (incl. under/overflow)
};

std::vector<int> varOffset;    // start of each variable in EvtData::counts
int nCountsTotal = 0;

///Auxiliary functions

//Read one file: per-event counts for all variables, keyed by (run, lumi, event)
bool readSample(const TString& fileName, int cent_min, int cent_max, std::map<EvtKey, EvtData>& events)
{
  TFile* f = TFile::Open(fileName);
  if (!f || f->IsZombie() || !f->Get(treeName)) {
    std::cerr << "ERROR: " << treeName << " not found in " << fileName
              << ". For sample=MC_RecoDebug, rerun cmsRun with saveTree=True." << std::endl;
    return false;
  }
  TTreeReader r(treeName, f);
  TTreeReaderValue<int> run(r, "nRun"), lumi(r, "nLumi"), evt(r, "nEv"), cent(r, "centrality");
  TTreeReaderArray<float> pt(r, "trkPt"), eta(r, "trkEta"), phi(r, "trkPhi"), chi2(r, "trkChi2"),
                          ptErr(r, "trkPtError"), dz(r, "trkDz1"), dzErr(r, "trkDzError1"),
                          dxy(r, "trkDxy1"), dxyErr(r, "trkDxyError1");
  TTreeReaderArray<UChar_t> nhit(r, "trkNHit"), algo(r, "trkAlgo"), ndof(r, "trkNdof"), nlay(r, "trkNlayer");
  TTreeReaderArray<bool> hp(r, "highPurity");

  std::vector<TAxis> axes;
  for (auto& v : trkVars) axes.emplace_back(v.nbins, v.xmin, v.xmax);

  int nDuplicates = 0;
  while (r.Next()) {
    EvtKey key = {*run, *lumi, *evt};
    if (events.count(key)) { nDuplicates++; continue; }
    EvtData& ed = events[key];
    ed.passCent = (*cent >= 2 * cent_min && *cent < 2 * cent_max);
    ed.counts.assign(nCountsTotal, 0.f);
    if (!ed.passCent) continue;

    for (size_t i = 0; i < pt.GetSize(); i++) {
      if (pt[i] < ptMin || pt[i] > ptMax) continue;
      if (hpOnly && !hp[i]) continue;
      double val[kNVars];
      val[kPt]     = pt[i];
      val[kEta]    = eta[i];
      val[kPhi]    = phi[i];
      val[kNHit]   = nhit[i];
      val[kChi2]   = (ndof[i] > 0 && nlay[i] > 0) ? chi2[i] / ndof[i] / nlay[i] : -1.;
      val[kAlgo]   = algo[i];
      val[kPtRes]  = ptErr[i] / pt[i];
      val[kDzSig]  = dz[i] / dzErr[i];
      val[kDxySig] = dxy[i] / dxyErr[i];
      for (int k = 0; k < kNVars; k++) {
        if (!std::isfinite(val[k])) continue;
        ed.counts[varOffset[k] + axes[k].FindFixBin(val[k])] += 1.f;
      }
    }
  }
  if (nDuplicates) std::cout << "WARNING: " << nDuplicates << " duplicated events in " << fileName << " were skipped" << std::endl;
  f->Close();
  return true;
}

//Draw one comparison (distribution + given ratio) inside the currently active pad
void drawPanel(TH1D* h1, TH1D* h2, TH1D* r, const TrkVar& v, const TString& label1, const TString& label2, const TString& centText)
{
  TVirtualPad* parent = gPad;
  TString id = Form("%s_%p", v.name, (void*) parent);

  TPad* pTop = new TPad("top_" + id, "", 0, 0.30, 1, 1);
  TPad* pBot = new TPad("bot_" + id, "", 0, 0, 1, 0.30);
  pTop->SetBottomMargin(0.0); pTop->SetLeftMargin(0.16); pTop->SetRightMargin(0.04); pTop->SetTopMargin(0.06);
  pBot->SetTopMargin(0.0);    pBot->SetLeftMargin(0.16); pBot->SetRightMargin(0.04); pBot->SetBottomMargin(0.38);
  pTop->SetTicks(1, 1); pBot->SetTicks(1, 1);
  pTop->SetLogy();
  pTop->Draw(); pBot->Draw();

  // --- top: distributions
  pTop->cd();
  h1->SetMarkerStyle(24); h1->SetMarkerColor(kBlue);  h1->SetLineColor(kBlue);  h1->SetMarkerSize(0.9);
  h2->SetMarkerStyle(25); h2->SetMarkerColor(kRed);   h2->SetLineColor(kRed);   h2->SetMarkerSize(0.9);

  double ymax = std::max(h1->GetMaximum(), h2->GetMaximum());
  double ymin = 1e30;
  for (TH1D* h : {h1, h2})
    for (int b = 1; b <= h->GetNbinsX(); b++)
      if (h->GetBinContent(b) > 0) ymin = std::min(ymin, h->GetBinContent(b));
  if (ymin > ymax) ymin = 1e-3 * ymax;

  h1->SetTitle("");
  h1->GetYaxis()->SetTitle(yAxisTitle);
  h1->GetYaxis()->SetRangeUser(0.5 * ymin, 5.0 * ymax);
  h1->GetYaxis()->SetTitleSize(0.065); h1->GetYaxis()->SetTitleOffset(1.15); h1->GetYaxis()->SetLabelSize(0.055);
  h1->GetXaxis()->SetLabelSize(0);
  h1->Draw("E1");
  h2->Draw("E1 same");
  h1->Draw("E1 same"); // sample1 on top

  TLatex tl; tl.SetNDC(); tl.SetTextFont(42); tl.SetTextSize(0.05);
  tl.DrawLatex(0.60, 0.86, centText);
  TLegend* leg = new TLegend(0.58, 0.68, 0.94, 0.84);
  leg->SetBorderSize(0); leg->SetFillStyle(0); leg->SetTextFont(42); leg->SetTextSize(0.05);
  leg->AddEntry(h1, label1, "lep");
  leg->AddEntry(h2, label2, "lep");
  leg->Draw();

  // --- bottom: ratio sample1/sample2 (errors already set)
  pBot->cd();
  r->SetMarkerStyle(24); r->SetMarkerColor(kBlue); r->SetLineColor(kBlue);
  r->GetYaxis()->SetRangeUser(ratioMin, ratioMax);
  r->GetYaxis()->SetTitle("Ratio");
  r->GetYaxis()->SetNdivisions(504);
  r->GetYaxis()->SetTitleSize(0.14); r->GetYaxis()->SetTitleOffset(0.5); r->GetYaxis()->SetLabelSize(0.12);
  r->GetXaxis()->SetTitle(v.xtitle);
  r->GetXaxis()->SetTitleSize(0.16); r->GetXaxis()->SetTitleOffset(1.0); r->GetXaxis()->SetLabelSize(0.13);
  r->GetXaxis()->SetLabelOffset(0.02);
  r->Draw("E1");
  TLine* line = new TLine(r->GetXaxis()->GetXmin(), 1.0, r->GetXaxis()->GetXmax(), 1.0);
  line->SetLineStyle(2); line->Draw();

  parent->cd();
}

void plotTrackQuantities_TwoSamples(TString fileName1 = "trk1.root", TString fileName2 = "trk2.root",
                                    TString label1 = "Sample 1", TString label2 = "Sample 2",
                                    int cent_min = 0, int cent_max = 10, TString outDir = "",
                                    TString norm = "none")
{
  gStyle->SetOptStat(0);
  gStyle->SetErrorX(0);

  if      (norm == "none")  yAxisTitle = "Number of tracks";
  else if (norm == "event") yAxisTitle = "Tracks per event";
  else if (norm == "area")  yAxisTitle = "Normalized distribution";
  else { std::cerr << "ERROR: normalization must be none, event or area (got " << norm << ")" << std::endl; return; }

  if (outDir == "") outDir = Form("TrackQuantities_cent%dto%d_%s", cent_min, cent_max, norm.Data());
  gSystem->Exec("mkdir -p " + outDir);
  TString centText = Form("Cent:%d-%d%%", cent_min, cent_max);

  varOffset.clear(); nCountsTotal = 0;
  for (auto& v : trkVars) { varOffset.push_back(nCountsTotal); nCountsTotal += v.nbins + 2; }

  // ---- read both samples
  std::map<EvtKey, EvtData> ev1, ev2;
  std::cout << "Reading " << fileName1 << " ..." << std::endl;
  if (!readSample(fileName1, cent_min, cent_max, ev1)) return;
  std::cout << "Reading " << fileName2 << " ..." << std::endl;
  if (!readSample(fileName2, cent_min, cent_max, ev2)) return;

  // ---- event bookkeeping
  std::set<EvtKey> allKeys;
  for (auto& e : ev1) allKeys.insert(e.first);
  for (auto& e : ev2) allKeys.insert(e.first);
  long nCommon = 0, nPass1 = 0, nPass2 = 0, nPassBoth = 0;
  for (auto& k : allKeys) {
    auto i1 = ev1.find(k), i2 = ev2.find(k);
    bool in1 = i1 != ev1.end(), in2 = i2 != ev2.end();
    if (in1 && in2) nCommon++;
    bool p1 = in1 && i1->second.passCent, p2 = in2 && i2->second.passCent;
    nPass1 += p1; nPass2 += p2; nPassBoth += (p1 && p2);
  }
  std::cout << "Events: " << ev1.size() << " (" << label1 << "), " << ev2.size() << " (" << label2 << "), "
            << nCommon << " in both" << std::endl;
  std::cout << "In " << centText << ": " << nPass1 << " (" << label1 << "), " << nPass2 << " (" << label2 << "), "
            << nPassBoth << " in both" << std::endl;
  if ((long) ev1.size() != nCommon || (long) ev2.size() != nCommon)
    std::cout << "WARNING: the files do not contain the same events. Unmatched events count as zero in the other"
              << " sample, which inflates the ratio uncertainty. Use eventList in cmsRun to process common events only." << std::endl;
  if (nPass1 != nPassBoth || nPass2 != nPassBoth)
    std::cout << "NOTE: some events fall in this centrality range in only one sample (centrality is reconstructed"
              << " separately in each release); this migration is included in the ratio and its uncertainty." << std::endl;
  if (nPass1 == 0 || nPass2 == 0) { std::cerr << "ERROR: no events in this centrality range" << std::endl; return; }

  const std::vector<float> zeros(nCountsTotal, 0.f);
  const double N = allKeys.size();

  std::vector<TH1D*> h1s, h2s, rs;
  TFile fout(outDir + "/trackQuantities.root", "RECREATE");
  std::cout << "\nRatio uncertainty, median over filled bins: correlated (paired) / uncorrelated" << std::endl;

  for (int k = 0; k < kNVars; k++) {
    const TrkVar& v = trkVars[k];
    const int off = varOffset[k], nb = v.nbins;

    // sums over events
    std::vector<double> S1(nb + 2, 0.), S2(nb + 2, 0.);
    double T1 = 0., T2 = 0.;                           // in-range totals (area normalization)
    for (auto& e : ev1) for (int b = 1; b <= nb; b++) { S1[b] += e.second.counts[off + b]; T1 += e.second.counts[off + b]; }
    for (auto& e : ev2) for (int b = 1; b <= nb; b++) { S2[b] += e.second.counts[off + b]; T2 += e.second.counts[off + b]; }
    const double D1 = (norm == "event") ? nPass1 : T1;
    const double D2 = (norm == "event") ? nPass2 : T2;
    const bool useD = (norm != "none");

    // per-bin sums of squared influence terms
    std::vector<double> Z2(nb + 2, 0.);
    for (auto& key : allKeys) {
      auto i1 = ev1.find(key), i2 = ev2.find(key);
      const std::vector<float>& c1 = (i1 != ev1.end()) ? i1->second.counts : zeros;
      const std::vector<float>& c2 = (i2 != ev2.end()) ? i2->second.counts : zeros;
      double dterm = 0.;
      if (useD) {
        double d1 = 0., d2 = 0.;
        if (norm == "event") {
          d1 = (i1 != ev1.end() && i1->second.passCent) ? 1. : 0.;
          d2 = (i2 != ev2.end() && i2->second.passCent) ? 1. : 0.;
        } else {
          for (int b = 1; b <= nb; b++) { d1 += c1[off + b]; d2 += c2[off + b]; }
        }
        dterm = (D1 > 0 ? d1 / D1 : 0.) - (D2 > 0 ? d2 / D2 : 0.);
      }
      for (int b = 1; b <= nb; b++) {
        if (S1[b] <= 0 || S2[b] <= 0) continue;
        double z = c1[off + b] / S1[b] - c2[off + b] / S2[b] - dterm;
        Z2[b] += z * z;
      }
    }

    // histograms
    TH1D* h1 = new TH1D(Form("h_%s_s1", v.name), "", nb, v.xmin, v.xmax);
    TH1D* h2 = new TH1D(Form("h_%s_s2", v.name), "", nb, v.xmin, v.xmax);
    TH1D* r  = new TH1D(Form("ratio_%s", v.name), "", nb, v.xmin, v.xmax);
    for (TH1D* h : {h1, h2, r}) h->SetDirectory(0);
    std::vector<double> errRatio;
    for (int b = 1; b <= nb; b++) {
      h1->SetBinContent(b, S1[b]); h1->SetBinError(b, std::sqrt(S1[b]));
      h2->SetBinContent(b, S2[b]); h2->SetBinError(b, std::sqrt(S2[b]));
    }
    if (norm == "event") { h1->Scale(1.0 / nPass1); h2->Scale(1.0 / nPass2); }
    if (norm == "area")  { if (T1 > 0) h1->Scale(1.0 / T1, "width"); if (T2 > 0) h2->Scale(1.0 / T2, "width"); }

    for (int b = 1; b <= nb; b++) {
      if (S1[b] <= 0 || S2[b] <= 0) { r->SetBinContent(b, -10); r->SetBinError(b, 0); continue; } // hidden
      double R = h1->GetBinContent(b) / h2->GetBinContent(b);
      double eCorr   = R * std::sqrt(Z2[b] * (N > 1 ? N / (N - 1) : 1.));
      double eUncorr = R * std::sqrt(1. / S1[b] + 1. / S2[b]);
      r->SetBinContent(b, R); r->SetBinError(b, eCorr);
      if (eUncorr > 0) errRatio.push_back(eCorr / eUncorr);
    }
    if (!errRatio.empty()) {
      std::sort(errRatio.begin(), errRatio.end());
      printf("  %-8s %.3f\n", v.name, errRatio[errRatio.size() / 2]);
    }

    fout.cd(); h1->Write(); h2->Write(); r->Write();
    h1s.push_back(h1); h2s.push_back(h2); rs.push_back(r);
  }
  fout.Close();

  // ---- one canvas per quantity
  for (int k = 0; k < kNVars; k++) {
    TCanvas* c = new TCanvas(Form("c_%s", trkVars[k].name), "", 600, 650);
    c->cd();
    drawPanel(h1s[k], h2s[k], rs[k], trkVars[k], label1, label2, centText);
    c->Print(outDir + "/" + trkVars[k].name + ".pdf");
    c->Print(outDir + "/" + trkVars[k].name + ".png");
  }

  // ---- 3x3 summary
  TCanvas* cs = new TCanvas("c_summary", "", 1800, 1950);
  cs->Divide(3, 3, 0.002, 0.002);
  for (int k = 0; k < kNVars; k++) {
    cs->cd(k + 1);
    TH1D* a = (TH1D*) h1s[k]->Clone(); a->SetDirectory(0);
    TH1D* b = (TH1D*) h2s[k]->Clone(); b->SetDirectory(0);
    TH1D* q = (TH1D*) rs[k]->Clone();  q->SetDirectory(0);
    drawPanel(a, b, q, trkVars[k], label1, label2, centText);
  }
  cs->Print(outDir + "/TrackQuantities_summary.pdf");
  cs->Print(outDir + "/TrackQuantities_summary.png");

  std::cout << "Plots saved in " << outDir << "/" << std::endl;
}
