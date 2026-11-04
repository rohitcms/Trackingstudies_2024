///Compare track-level quantities between two samples, in the style:
///  top pad    : distribution (log y), sample1 = blue open circles, sample2 = red open squares
///  bottom pad : ratio sample1/sample2 (0-2), dashed line at 1
///Quantities: pT, eta, phi, number of hits, chi2/ndf/nlayers, algo, pTErr/pT, dz/sigma_dz, dxy/sigma_dxy
///
///Input: files with the anaTrack tree ("anaTrack/trackTree"), e.g. from
///  cmsRun run_PbPb_cfg.py sample=MC_RecoDebug saveTree=True ...
///(or any of the AOD/MiniAOD samples, which always write the tree)
///
///Run:
///  root -l -b -q 'plotTrackQuantities_TwoSamples.C("trk_rereco.root","trk_prompt.root","ReReco","Prompt",0,10)'
///Arguments: file1, file2, label1, label2, cent_min, cent_max (in %), [output directory], [normalization]
///Normalization: "none"  = raw number of tracks per bin (default; use when both samples have the same events)
///               "event" = tracks per event per bin
///               "area"  = unit area per unit x (shape-only comparison)
///e.g. root -l -b -q 'plotTrackQuantities_TwoSamples.C("a.root","b.root","A","B",0,10,"","area")'
///Output: one PDF+PNG per quantity, a 3x3 summary (TrackQuantities_summary.pdf/png) and all histograms in a ROOT file.

#include "ROOT/RDataFrame.hxx"
#include "ROOT/RVec.hxx"
#include "TFile.h"
#include "TTree.h"
#include "TH1D.h"
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
#include <algorithm>

///*** Important parameters
const char* treeName = "anaTrack/trackTree";
bool  hpOnly = true;      // use only highPurity tracks (as in the reference plots)
float ptMin  = 0.0;       // pT range of the selected tracks (GeV)
float ptMax  = 99999.;

struct TrkVar {
  const char* column;   // RDataFrame column defined below
  const char* name;     // histogram / file name
  const char* xtitle;
  int nbins; double xmin, xmax;
};

const std::vector<TrkVar> trkVars = {
  {"v_pt",     "pt",       "p_{T} (GeV)",           100,   0.0,  10.0},
  {"v_eta",    "eta",      "#eta",                   60,  -3.0,   3.0},
  {"v_phi",    "phi",      "#phi",                   64, -TMath::Pi(), TMath::Pi()},
  {"v_nhit",   "nhits",    "Number Of Hits",         50,   0.0,  50.0},
  {"v_chi2",   "chi2",     "#chi^{2}/ndf/nlayers",  100,   0.0,   1.0},
  {"v_algo",   "algo",     "Algo",                   30,   0.0,  30.0},
  {"v_ptres",  "ptres",    "p_{T} Err/p_{T}",       100,   0.0,   0.3},
  {"v_dzsig",  "dzsig",    "dZ/#sigma_{dZ}",        100, -30.0,  30.0},
  {"v_dxysig", "dxysig",   "dXY/#sigma_{dXY}",      100, -30.0,  30.0},
};

TString yAxisTitle = "Number of tracks"; // set from the normalization option

///Auxiliary functions

//Define the selected-track mask and all plotted quantities on a dataframe
ROOT::RDF::RNode defineVars(ROOT::RDF::RNode df)
{
  TString selExpr = Form("ROOT::RVecI m = (trkPt >= %f) && (trkPt <= %f); %s return m;",
                         ptMin, ptMax, hpOnly ? "m = m && highPurity;" : "");
  return df.Define("sel", selExpr.Data())
           .Define("v_pt",  "ROOT::RVecF(trkPt[sel])")
           .Define("v_eta", "ROOT::RVecF(trkEta[sel])")
           .Define("v_phi", "ROOT::RVecF(trkPhi[sel])")
           .Define("v_nhit", "auto x = trkNHit[sel]; return ROOT::RVecF(x.begin(), x.end());")
           .Define("v_algo", "auto x = trkAlgo[sel]; return ROOT::RVecF(x.begin(), x.end());")
           .Define("v_chi2", "auto c = trkChi2[sel]; auto n = trkNdof[sel]; auto l = trkNlayer[sel];"
                             "ROOT::RVecF r(c.size());"
                             "for (size_t i = 0; i < c.size(); i++) r[i] = (n[i] > 0 && l[i] > 0) ? c[i]/n[i]/l[i] : -1.f;"
                             "return r;")
           .Define("v_ptres",  "ROOT::RVecF(trkPtError[sel]/trkPt[sel])")
           .Define("v_dzsig",  "ROOT::RVecF(trkDz1[sel]/trkDzError1[sel])")
           .Define("v_dxysig", "ROOT::RVecF(trkDxy1[sel]/trkDxyError1[sel])")
           .Define("nSelTrk",  "(int) v_pt.size()");
}

//Draw one comparison (distribution + ratio) inside the currently active pad
void drawPanel(TH1D* h1, TH1D* h2, const TrkVar& v, const TString& label1, const TString& label2, const TString& centText)
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

  // --- top: normalized distributions
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

  // --- bottom: ratio sample1/sample2
  pBot->cd();
  TH1D* r = (TH1D*) h1->Clone(TString("ratio_") + v.name + "_" + id);
  r->SetDirectory(0);
  r->Divide(h2);
  for (int b = 1; b <= r->GetNbinsX(); b++) // hide bins empty in either sample
    if (h1->GetBinContent(b) <= 0 || h2->GetBinContent(b) <= 0) { r->SetBinContent(b, -10); r->SetBinError(b, 0); }
  r->SetMarkerStyle(24); r->SetMarkerColor(kBlue); r->SetLineColor(kBlue);
  r->GetYaxis()->SetRangeUser(0.0, 2.0);
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
  if (outDir == "") outDir = Form("TrackQuantities_cent%dto%d_%s", cent_min, cent_max, norm.Data());
  gSystem->Exec("mkdir -p " + outDir);

  // check inputs contain the track tree
  for (TString fn : {fileName1, fileName2}) {
    TFile* f = TFile::Open(fn);
    if (!f || f->IsZombie() || !f->Get(treeName)) {
      std::cerr << "ERROR: " << treeName << " not found in " << fn
                << ". For sample=MC_RecoDebug, rerun cmsRun with saveTree=True." << std::endl;
      return;
    }
    f->Close();
  }

  // centrality: hiBin = 2 x centrality(%), 0-199
  TString centCut  = Form("centrality >= %d && centrality < %d", 2 * cent_min, 2 * cent_max);
  TString centText = Form("Cent:%d-%d%%", cent_min, cent_max);

  ROOT::RDataFrame d1(treeName, fileName1.Data());
  ROOT::RDataFrame d2(treeName, fileName2.Data());
  auto s1 = defineVars(d1.Filter(centCut.Data(), "centrality"));
  auto s2 = defineVars(d2.Filter(centCut.Data(), "centrality"));

  // book all histograms first, so each file is read only once
  std::vector<ROOT::RDF::RResultPtr<TH1D>> H1, H2;
  for (auto& v : trkVars) {
    H1.push_back(s1.Histo1D({Form("h_%s_s1", v.name), "", v.nbins, v.xmin, v.xmax}, v.column));
    H2.push_back(s2.Histo1D({Form("h_%s_s2", v.name), "", v.nbins, v.xmin, v.xmax}, v.column));
  }
  auto nEv1 = s1.Count();       auto nEv2 = s2.Count();
  auto nTrk1 = s1.Sum<int>("nSelTrk"); auto nTrk2 = s2.Sum<int>("nSelTrk");

  std::cout << label1 << ": " << *nEv1 << " events, " << *nTrk1 << " selected tracks in " << centText << std::endl;
  std::cout << label2 << ": " << *nEv2 << " events, " << *nTrk2 << " selected tracks in " << centText << std::endl;
  if (*nEv1 == 0 || *nEv2 == 0) { std::cerr << "ERROR: no events in this centrality range" << std::endl; return; }
  if (*nEv1 != *nEv2)
    std::cout << "WARNING: different number of events in the two samples; check they contain the same events." << std::endl;

  // normalization
  if      (norm == "none")  yAxisTitle = "Number of tracks";
  else if (norm == "event") yAxisTitle = "Tracks per event";
  else if (norm == "area")  yAxisTitle = "Normalized distribution";
  else { std::cerr << "ERROR: normalization must be none, event or area (got " << norm << ")" << std::endl; return; }
  std::cout << "Normalization: " << norm << std::endl;

  std::vector<TH1D*> h1s, h2s;
  for (size_t i = 0; i < trkVars.size(); i++) {
    TH1D* h1 = (TH1D*) H1[i]->Clone(); h1->SetDirectory(0);
    TH1D* h2 = (TH1D*) H2[i]->Clone(); h2->SetDirectory(0);
    if (norm == "event") {
      h1->Scale(1.0 / (double) *nEv1);
      h2->Scale(1.0 / (double) *nEv2);
    } else if (norm == "area") {
      if (h1->Integral() > 0) h1->Scale(1.0 / h1->Integral(), "width");
      if (h2->Integral() > 0) h2->Scale(1.0 / h2->Integral(), "width");
    }
    h1s.push_back(h1); h2s.push_back(h2);
  }

  // one canvas per quantity
  for (size_t i = 0; i < trkVars.size(); i++) {
    TCanvas* c = new TCanvas(Form("c_%s", trkVars[i].name), "", 600, 650);
    c->cd();
    drawPanel(h1s[i], h2s[i], trkVars[i], label1, label2, centText);
    c->Print(outDir + "/" + trkVars[i].name + ".pdf");
    c->Print(outDir + "/" + trkVars[i].name + ".png");
  }

  // 3x3 summary
  TCanvas* cs = new TCanvas("c_summary", "", 1800, 1950);
  cs->Divide(3, 3, 0.002, 0.002);
  for (size_t i = 0; i < trkVars.size(); i++) {
    cs->cd(i + 1);
    TH1D* a = (TH1D*) h1s[i]->Clone(); a->SetDirectory(0);
    TH1D* b = (TH1D*) h2s[i]->Clone(); b->SetDirectory(0);
    drawPanel(a, b, trkVars[i], label1, label2, centText);
  }
  cs->Print(outDir + "/TrackQuantities_summary.pdf");
  cs->Print(outDir + "/TrackQuantities_summary.png");

  TFile fout(outDir + "/trackQuantities.root", "RECREATE");
  for (size_t i = 0; i < trkVars.size(); i++) { h1s[i]->Write(); h2s[i]->Write(); }
  fout.Close();

  std::cout << "Plots saved in " << outDir << "/" << std::endl;
}
