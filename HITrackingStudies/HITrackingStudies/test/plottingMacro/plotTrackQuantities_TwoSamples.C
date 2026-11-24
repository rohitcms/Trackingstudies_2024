///Compare track-level quantities between two samples that contain the SAME events
///(e.g. the same GEN-SIM/DIGI reconstructed with two releases). No normalization: raw track counts.
///
///Output, rows of three panels (ready for slides), each panel = distribution (log y) + ratio sample1/sample2:
///  TrackKinematics : pT, eta, phi
///  TrackHits       : number of hits, chi2/ndf/nlayers, algo
///  TrackResolution : pTErr/pT, dz/sigma_dz, dxy/sigma_dxy
///  TrackPixel      : number of pixel layers, number of pixel hits, number of tracker layers
///and all histograms in trackQuantities.root.
///Sample1 = blue open circles, sample2 = red open squares.
///
///Uncertainties (events are the independent units; events matched by run, lumi, event):
///  counts S = sum_e n_e        : Var(S) = n/(n-1) * sum_e (n_e - S/n)^2   (event-to-event spread)
///  ratio R = S1/S2 (paired)     : Var(R) = R^2 * n/(n-1) * sum_e (n1_e/S1 - n2_e/S2)^2
///In the ratio, fluctuations common to both samples cancel and only genuine reconstruction
///differences remain. The macro prints the median (paired / unpaired) ratio uncertainty.
///
///Input: files with the anaTrack tree ("anaTrack/trackTree"), from
///  cmsRun run_PbPb_cfg.py sample=MC_RecoDebug saveTree=True ...
///Run:
///  root -l -b -q 'plotTrackQuantities_TwoSamples.C("trk_rereco.root","trk_prompt.root","ReReco","Prompt",0,10)'
///Arguments: file1, file2, label1, label2, cent_min, cent_max (in %), [output directory], [track type]
///Track type: "general" (default) = highPurity generalTracks; "pixel" = all tracks in the tree, for files made
///with cmsRun ... usePixelTrks=True (pixel tracks have no highPurity flag)

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
#include <memory>

// All helpers live in their own namespace: after cmsenv ROOT knows CMSSW classes with
// generic names (e.g. a global 'Binning'), which would otherwise clash.
namespace trkcmp {

///*** Important parameters
const char* treeName = "anaTrack/trackTree";
bool  hpOnly = true;        // use only highPurity tracks (set from the track type argument)
TString trackText = "";     // track-type label drawn on the plots
float ptMin  = 0.0;         // pT range of the selected tracks (GeV)
float ptMax  = 99999.;
double ratioHalfRange = -1; // ratio panel shown as 1 +- this; <= 0 means automatic

struct TrkVar { const char* name; const char* xtitle; int nbins; double xmin, xmax; };

enum { kPt, kEta, kPhi, kNHit, kChi2, kAlgo, kPtRes, kDzSig, kDxySig, kNPixLay, kNPixHit, kNLay, kNVars };

const std::vector<TrkVar> trkVars = {
  {"pt",     "p_{T} (GeV)",           100,   0.0,  10.0},
  {"eta",    "#eta",                   60,  -3.0,   3.0},
  {"phi",    "#phi",                   64, -TMath::Pi(), TMath::Pi()},
  {"nhits",  "Number of hits",         50,   0.0,  50.0},
  {"chi2",   "#chi^{2}/ndf/nlayers",  100,   0.0,   1.0},
  {"algo",   "Algo",                   30,   0.0,  30.0},
  {"ptres",  "p_{T} Err/p_{T}",       100,   0.0,   0.3},
  {"dzsig",  "dZ/#sigma_{dZ}",        100, -30.0,  30.0},
  {"dxysig", "dXY/#sigma_{dXY}",      100, -30.0,  30.0},
  {"npixlayers", "Number of pixel layers",   8, 0.0,  8.0},
  {"npixhits",   "Number of pixel hits",    12, 0.0, 12.0},
  {"nlayers",    "Number of tracker layers", 25, 0.0, 25.0},
};

struct Row { const char* fig; int vars[3]; };
const std::vector<Row> rows = {
  {"TrackKinematics", {kPt, kEta, kPhi}},
  {"TrackHits",       {kNHit, kChi2, kAlgo}},
  {"TrackResolution", {kPtRes, kDzSig, kDxySig}},
  {"TrackPixel",      {kNPixLay, kNPixHit, kNLay}},
};

typedef std::array<unsigned long long,3> EvtKey; // run, lumi, event

struct EvtData { bool passCent = false; std::vector<float> counts; }; // nbins+2 per variable, concatenated

std::vector<int> varOffset;
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
  TTree* t = (TTree*) f->Get(treeName);
  const bool hasPixHit = t->GetBranch("trkNPixHit") != nullptr;
  if (!hasPixHit) std::cout << "WARNING: no trkNPixHit branch in " << fileName
                            << " (made with an older TrackAnalyzer); the pixel-hit plot will be empty" << std::endl;
  TTreeReader r(t);
  TTreeReaderValue<int> run(r, "nRun"), lumi(r, "nLumi"), evt(r, "nEv"), cent(r, "centrality");
  TTreeReaderArray<float> pt(r, "trkPt"), eta(r, "trkEta"), phi(r, "trkPhi"), chi2(r, "trkChi2"),
                          ptErr(r, "trkPtError"), dz(r, "trkDz1"), dzErr(r, "trkDzError1"),
                          dxy(r, "trkDxy1"), dxyErr(r, "trkDxyError1");
  TTreeReaderArray<UChar_t> nhit(r, "trkNHit"), algo(r, "trkAlgo"), ndof(r, "trkNdof"), nlay(r, "trkNlayer");
  TTreeReaderArray<UChar_t> npixlay(r, "trkNpixellayer");
  std::unique_ptr<TTreeReaderArray<UChar_t>> npixhit;
  if (hasPixHit) npixhit.reset(new TTreeReaderArray<UChar_t>(r, "trkNPixHit"));
  TTreeReaderArray<bool> hp(r, "highPurity");

  std::vector<TAxis> axes;
  for (auto& v : trkVars) axes.emplace_back(v.nbins, v.xmin, v.xmax);

  int nDuplicates = 0;
  while (r.Next()) {
    // nEv is a 32-bit int in the tree; cast through unsigned so large event numbers still match
    EvtKey key = {(unsigned long long) (unsigned) *run, (unsigned long long) (unsigned) *lumi, (unsigned long long) (unsigned) *evt};
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
      val[kNPixLay] = npixlay[i];
      val[kNPixHit] = hasPixHit ? (double) (*npixhit)[i] : std::nan("");
      val[kNLay]    = nlay[i];
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

double median(std::vector<double> v) { if (v.empty()) return -1; std::sort(v.begin(), v.end()); return v[v.size() / 2]; }

//Draw one panel (distribution + ratio) in the current pad
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

  pTop->cd();
  h1->SetMarkerStyle(24); h1->SetMarkerColor(kBlue); h1->SetLineColor(kBlue); h1->SetMarkerSize(0.9);
  h2->SetMarkerStyle(25); h2->SetMarkerColor(kRed);  h2->SetLineColor(kRed);  h2->SetMarkerSize(0.9);
  double ymax = std::max(h1->GetMaximum(), h2->GetMaximum());
  double ymin = 1e30;
  for (TH1D* h : {h1, h2})
    for (int b = 1; b <= h->GetNbinsX(); b++)
      if (h->GetBinContent(b) > 0) ymin = std::min(ymin, h->GetBinContent(b));
  if (ymin > ymax) ymin = 1e-3 * ymax;
  h1->SetTitle("");
  h1->GetYaxis()->SetTitle("Number of tracks");
  h1->GetYaxis()->SetRangeUser(0.5 * ymin, 8.0 * ymax);
  h1->GetYaxis()->SetTitleSize(0.06); h1->GetYaxis()->SetTitleOffset(1.25); h1->GetYaxis()->SetLabelSize(0.055);
  h1->GetXaxis()->SetLabelSize(0);
  h1->Draw("E1");
  h2->Draw("E1 same");
  h1->Draw("E1 same");

  TLatex tl; tl.SetNDC(); tl.SetTextFont(42); tl.SetTextSize(0.05);
  tl.DrawLatex(0.20, 0.86, centText);
  tl.DrawLatex(0.20, 0.79, trackText);
  TLegend* leg = new TLegend(0.55, 0.76, 0.94, 0.90);
  leg->SetBorderSize(0); leg->SetFillStyle(0); leg->SetTextFont(42); leg->SetTextSize(0.05);
  leg->AddEntry(h1, label1, "lep");
  leg->AddEntry(h2, label2, "lep");
  leg->Draw();

  pBot->cd();
  double half = ratioHalfRange;
  if (half <= 0) { // automatic: cover the points with their errors, symmetric around 1
    half = 0.02;
    for (int b = 1; b <= r->GetNbinsX(); b++)
      if (r->GetBinContent(b) >= 0) half = std::max(half, std::fabs(r->GetBinContent(b) - 1) + r->GetBinError(b));
    half = std::min(1.0, 1.15 * half);
  }
  r->SetMarkerStyle(24); r->SetMarkerColor(kBlue); r->SetLineColor(kBlue);
  r->GetYaxis()->SetRangeUser(1 - half, 1 + half);
  r->GetYaxis()->SetTitle("Ratio");
  r->GetYaxis()->SetNdivisions(504);
  r->GetYaxis()->SetTitleSize(0.13); r->GetYaxis()->SetTitleOffset(0.55); r->GetYaxis()->SetLabelSize(0.12);
  r->GetXaxis()->SetTitle(v.xtitle);
  r->GetXaxis()->SetTitleSize(0.15); r->GetXaxis()->SetTitleOffset(1.0); r->GetXaxis()->SetLabelSize(0.12);
  r->GetXaxis()->SetLabelOffset(0.02);
  r->Draw("E1");
  TLine* line = new TLine(r->GetXaxis()->GetXmin(), 1.0, r->GetXaxis()->GetXmax(), 1.0);
  line->SetLineStyle(2); line->Draw();

  parent->cd();
}

void run(TString fileName1 = "trk1.root", TString fileName2 = "trk2.root",
                                    TString label1 = "Sample 1", TString label2 = "Sample 2",
                                    int cent_min = 0, int cent_max = 10, TString outDir = "",
                                    TString trackType = "general")
{
  if (trackType != "general" && trackType != "pixel") { std::cerr << "ERROR: track type must be general or pixel" << std::endl; return; }
  hpOnly = (trackType == "general");
  trackText = hpOnly ? "highPurity general tracks" : "Pixel tracks";
  gStyle->SetOptStat(0);
  gStyle->SetErrorX(0);
  if (outDir == "") outDir = Form("TrackQuantities_cent%dto%d", cent_min, cent_max);
  gSystem->Exec("mkdir -p " + outDir);
  TString centText = Form("Cent: %d-%d%%", cent_min, cent_max);

  varOffset.clear(); nCountsTotal = 0;
  for (auto& v : trkVars) { varOffset.push_back(nCountsTotal); nCountsTotal += v.nbins + 2; }

  std::map<EvtKey, EvtData> ev1, ev2;
  std::cout << "Reading " << fileName1 << " ..." << std::endl;
  if (!readSample(fileName1, cent_min, cent_max, ev1)) return;
  std::cout << "Reading " << fileName2 << " ..." << std::endl;
  if (!readSample(fileName2, cent_min, cent_max, ev2)) return;

  // ---- event bookkeeping
  std::set<EvtKey> keys;
  for (auto& e : ev1) keys.insert(e.first);
  for (auto& e : ev2) keys.insert(e.first);
  long nCommon = 0, nPass1 = 0, nPass2 = 0, nPassBoth = 0;
  for (auto& k : keys) {
    auto i1 = ev1.find(k), i2 = ev2.find(k);
    bool in1 = i1 != ev1.end(), in2 = i2 != ev2.end();
    bool p1 = in1 && i1->second.passCent, p2 = in2 && i2->second.passCent;
    nCommon += (in1 && in2); nPass1 += p1; nPass2 += p2; nPassBoth += (p1 && p2);
  }
  std::cout << "Events: " << ev1.size() << " (" << label1 << "), " << ev2.size() << " (" << label2 << "), " << nCommon << " in both" << std::endl;
  std::cout << "In " << centText << ": " << nPass1 << " (" << label1 << "), " << nPass2 << " (" << label2 << "), " << nPassBoth << " in both" << std::endl;
  if ((long) ev1.size() != nCommon || (long) ev2.size() != nCommon)
    std::cout << "WARNING: the files do not contain the same events; unmatched events count as zero in the other sample."
              << " Use eventList in cmsRun to process common events only." << std::endl;
  if (nPass1 != nPassBoth || nPass2 != nPassBoth)
    std::cout << "NOTE: some events are in this centrality range in only one sample (centrality is reconstructed"
              << " separately in each release); this is included in the ratio and its uncertainty." << std::endl;
  if (nPass1 == 0 || nPass2 == 0) { std::cerr << "ERROR: no events in this centrality range" << std::endl; return; }

  const std::vector<float> zeros(nCountsTotal, 0.f);
  const double nEv = keys.size(), fN = nEv > 1 ? nEv / (nEv - 1) : 1.;
  const double n1 = ev1.size(), n2 = ev2.size();

  std::vector<TH1D*> h1s, h2s, rs;
  TFile fout(outDir + "/trackQuantities.root", "RECREATE");
  std::cout << "\nRatio uncertainty, median over filled bins: paired / unpaired" << std::endl;
  long sel1 = 0, sel2 = 0;

  for (int k = 0; k < kNVars; k++) {
    const TrkVar& v = trkVars[k];
    const int off = varOffset[k], nb = v.nbins;
    std::vector<double> S1(nb + 2, 0.), S2(nb + 2, 0.);
    for (auto& e : ev1) for (int b = 0; b <= nb + 1; b++) S1[b] += e.second.counts[off + b];
    for (auto& e : ev2) for (int b = 0; b <= nb + 1; b++) S2[b] += e.second.counts[off + b];
    if (k == kPt) { for (int b = 0; b <= nb + 1; b++) { sel1 += S1[b]; sel2 += S2[b]; } }

    // event-to-event spread of the counts (single samples) and paired ratio terms
    std::vector<double> V1(nb + 2, 0.), V2(nb + 2, 0.), ZR(nb + 2, 0.);
    for (auto& key : keys) {
      auto i1 = ev1.find(key), i2 = ev2.find(key);
      const std::vector<float>& c1 = (i1 != ev1.end()) ? i1->second.counts : zeros;
      const std::vector<float>& c2 = (i2 != ev2.end()) ? i2->second.counts : zeros;
      for (int b = 1; b <= nb; b++) {
        if (i1 != ev1.end()) V1[b] += std::pow(c1[off + b] - S1[b] / n1, 2);
        if (i2 != ev2.end()) V2[b] += std::pow(c2[off + b] - S2[b] / n2, 2);
        if (S1[b] > 0 && S2[b] > 0) ZR[b] += std::pow(c1[off + b] / S1[b] - c2[off + b] / S2[b], 2);
      }
    }

    TH1D* h1 = new TH1D(Form("h_%s_s1", v.name), "", nb, v.xmin, v.xmax);
    TH1D* h2 = new TH1D(Form("h_%s_s2", v.name), "", nb, v.xmin, v.xmax);
    TH1D* r  = new TH1D(Form("ratio_%s", v.name), "", nb, v.xmin, v.xmax);
    for (TH1D* h : {h1, h2, r}) h->SetDirectory(0);
    std::vector<double> errRatio;
    for (int b = 1; b <= nb; b++) {
      double e1 = std::sqrt(V1[b] * (n1 > 1 ? n1 / (n1 - 1) : 1.));
      double e2 = std::sqrt(V2[b] * (n2 > 1 ? n2 / (n2 - 1) : 1.));
      h1->SetBinContent(b, S1[b]); h1->SetBinError(b, e1);
      h2->SetBinContent(b, S2[b]); h2->SetBinError(b, e2);
      if (S1[b] <= 0 || S2[b] <= 0) { r->SetBinContent(b, -10); r->SetBinError(b, 0); continue; } // hidden
      double R = S1[b] / S2[b];
      double eCorr = R * std::sqrt(ZR[b] * fN);
      double eUnc  = R * std::sqrt(std::pow(e1 / S1[b], 2) + std::pow(e2 / S2[b], 2));
      r->SetBinContent(b, R); r->SetBinError(b, eCorr);
      if (eUnc > 0) errRatio.push_back(eCorr / eUnc);
    }
    printf("  %-8s %.3f\n", v.name, median(errRatio));
    fout.cd(); h1->Write(); h2->Write(); r->Write();
    h1s.push_back(h1); h2s.push_back(h2); rs.push_back(r);
  }
  fout.Close();
  std::cout << "Selected tracks: " << sel1 << " (" << label1 << "), " << sel2 << " (" << label2 << "), ratio "
            << (sel2 > 0 ? (double) sel1 / sel2 : 0.) << std::endl;

  // ---- rows of three
  for (auto& row : rows) {
    TCanvas* c = new TCanvas(Form("c_%s", row.fig), "", 1800, 700);
    c->Divide(3, 1, 0.001, 0.001);
    for (int j = 0; j < 3; j++) {
      int k = row.vars[j];
      c->cd(j + 1);
      drawPanel(h1s[k], h2s[k], rs[k], trkVars[k], label1, label2, centText);
    }
    c->Print(outDir + "/" + row.fig + ".pdf");
  }
  std::cout << "Plots saved in " << outDir << "/" << std::endl;
}

} // namespace trkcmp

void plotTrackQuantities_TwoSamples(TString fileName1 = "trk1.root", TString fileName2 = "trk2.root",
                                    TString label1 = "Sample 1", TString label2 = "Sample 2",
                                    int cent_min = 0, int cent_max = 10, TString outDir = "",
                                    TString trackType = "general")
{
  trkcmp::run(fileName1, fileName2, label1, label2, cent_min, cent_max, outDir, trackType);
}
