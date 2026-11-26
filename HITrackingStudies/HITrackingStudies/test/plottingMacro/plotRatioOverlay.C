///Overlay the ratio histograms of up to three runs of plotTrackQuantities_TwoSamples.C, e.g.
///  2024/2023 with prompt reco  vs  2024/2023 with the offline-GT rereco
///to see which reconstruction brings the two years closer (closer to 1).
///
///Run:
///  root -l -b -q 'plotRatioOverlay.C("dirA/trackQuantities.root","Prompt reco","dirB/trackQuantities.root","ReReco, offline GT","","","outDir","2024 / 2023, normalized to unit area")'
///Arguments: fileA, labelA, fileB, labelB, [fileC, labelC], outDir, title
///Output: outDir/<row>.pdf with rows of three panels (same rows as the track-quantity plots), and a printout of
///the mean |ratio - 1| per quantity for each input (weighted by the inverse squared uncertainty).

#include "TFile.h"
#include "TH1D.h"
#include "TCanvas.h"
#include "TLegend.h"
#include "TLatex.h"
#include "TLine.h"
#include "TStyle.h"
#include "TSystem.h"
#include <iostream>
#include <vector>
#include <algorithm>
#include <cmath>

namespace ratiooverlay {

struct Var { const char* name; const char* xtitle; };
const std::vector<Var> vars = {
  {"pt", "p_{T} (GeV)"}, {"eta", "#eta"}, {"phi", "#phi"},
  {"nhits", "Number of hits"}, {"chi2", "#chi^{2}/ndf/nlayers"}, {"algo", "Algo"},
  {"ptres", "p_{T} Err/p_{T}"}, {"dzsig", "dZ/#sigma_{dZ}"}, {"dxysig", "dXY/#sigma_{dXY}"},
  {"npixlayers", "Number of pixel layers"}, {"npixhits", "Number of pixel hits"}, {"nlayers", "Number of tracker layers"},
};
struct Row { const char* fig; int v[3]; };
const std::vector<Row> rows = {
  {"RatioKinematics", {0, 1, 2}}, {"RatioHits", {3, 4, 5}}, {"RatioResolution", {6, 7, 8}}, {"RatioPixel", {9, 10, 11}},
};
const int colors[3]  = {kBlue + 1, kRed + 1, kGreen + 2};
const int markers[3] = {24, 25, 26};

void run(std::vector<TString> files, std::vector<TString> labels, TString outDir, TString title)
{
  gStyle->SetOptStat(0);
  gStyle->SetErrorX(0);
  gSystem->Exec("mkdir -p " + outDir);
  const int n = files.size();
  std::vector<std::vector<TH1D*>> h(n, std::vector<TH1D*>(vars.size(), nullptr));
  for (int i = 0; i < n; i++) {
    TFile* f = TFile::Open(files[i]);
    if (!f || f->IsZombie()) { std::cerr << "ERROR: cannot open " << files[i] << std::endl; return; }
    for (size_t k = 0; k < vars.size(); k++) {
      TH1D* r = (TH1D*) f->Get(Form("ratio_%s", vars[k].name));
      if (!r) continue;
      h[i][k] = (TH1D*) r->Clone(Form("r_%d_%s", i, vars[k].name));
      h[i][k]->SetDirectory(0);
    }
    f->Close();
  }

  // mean |R - 1| per quantity, weighted by 1/sigma^2
  std::cout << "Mean |ratio - 1| (weighted):" << std::endl;
  printf("  %-11s", ""); for (int i = 0; i < n; i++) printf(" %22s", labels[i].Data()); printf("\n");
  for (size_t k = 0; k < vars.size(); k++) {
    printf("  %-11s", vars[k].name);
    for (int i = 0; i < n; i++) {
      double sw = 0, swd = 0;
      if (h[i][k])
        for (int b = 1; b <= h[i][k]->GetNbinsX(); b++) {
          double c = h[i][k]->GetBinContent(b), e = h[i][k]->GetBinError(b);
          if (c < 0 || e <= 0) continue;
          sw += 1 / (e * e); swd += std::fabs(c - 1) / (e * e);
        }
      printf(" %22.4f", sw > 0 ? swd / sw : -1.);
    }
    printf("\n");
  }

  for (auto& row : rows) {
    TCanvas* c = new TCanvas(Form("c_%s", row.fig), "", 1800, 560);
    c->Divide(3, 1, 0.001, 0.001);
    for (int j = 0; j < 3; j++) {
      const int k = row.v[j];
      c->cd(j + 1);
      gPad->SetLeftMargin(0.15); gPad->SetRightMargin(0.04); gPad->SetBottomMargin(0.16); gPad->SetTopMargin(0.08);
      gPad->SetTicks(1, 1);
      double half = 0.02;
      for (int i = 0; i < n; i++) {
        if (!h[i][k]) continue;
        for (int b = 1; b <= h[i][k]->GetNbinsX(); b++)
          if (h[i][k]->GetBinContent(b) >= 0 && h[i][k]->GetBinError(b) < 0.5)
            half = std::max(half, std::fabs(h[i][k]->GetBinContent(b) - 1) + h[i][k]->GetBinError(b));
      }
      half = std::min(1.0, 1.15 * half);
      bool first = true;
      TLegend* leg = new TLegend(0.18, 0.72, 0.62, 0.90);
      leg->SetBorderSize(0); leg->SetFillStyle(0); leg->SetTextFont(42); leg->SetTextSize(0.045);
      for (int i = 0; i < n; i++) {
        TH1D* r = h[i][k];
        if (!r) continue;
        r->SetMarkerStyle(markers[i]); r->SetMarkerColor(colors[i]); r->SetLineColor(colors[i]); r->SetMarkerSize(0.9);
        if (first) {
          r->SetTitle("");
          r->GetYaxis()->SetRangeUser(1 - half, 1 + 1.6 * half);
          r->GetYaxis()->SetTitle("Ratio");
          r->GetXaxis()->SetTitle(vars[k].xtitle);
          r->GetXaxis()->SetTitleSize(0.055); r->GetYaxis()->SetTitleSize(0.055);
          r->GetXaxis()->SetLabelSize(0.045); r->GetYaxis()->SetLabelSize(0.045);
          r->Draw("E1");
          first = false;
        } else r->Draw("E1 same");
        leg->AddEntry(r, labels[i], "lep");
      }
      if (first) continue;
      TLine* line = new TLine(h[0][k] ? h[0][k]->GetXaxis()->GetXmin() : 0, 1.0, h[0][k] ? h[0][k]->GetXaxis()->GetXmax() : 1, 1.0);
      line->SetLineStyle(2); line->Draw();
      leg->Draw();
      TLatex tl; tl.SetNDC(); tl.SetTextFont(42); tl.SetTextSize(0.045);
      tl.DrawLatex(0.15, 0.94, title);
    }
    c->Print(outDir + "/" + row.fig + ".pdf");
  }
  std::cout << "Plots saved in " << outDir << "/" << std::endl;
}

} // namespace ratiooverlay

void plotRatioOverlay(TString fileA, TString labelA, TString fileB, TString labelB,
                      TString fileC = "", TString labelC = "", TString outDir = "RatioOverlay", TString title = "")
{
  std::vector<TString> files = {fileA, fileB}, labels = {labelA, labelB};
  if (fileC != "") { files.push_back(fileC); labels.push_back(labelC); }
  ratiooverlay::run(files, labels, outDir, title);
}
