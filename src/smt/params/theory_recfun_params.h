/*++
  Copyright (c) 2020 Arie Gurfinkel

  Module Name:

  theory_recfun_params.h

  Abstract:

  Parameters for the theory solver for recfun

  Author:

  Hari Govind V K 2020-12-03.

  Revision History:

  --*/
#pragma once
#include "util/params.h"

struct theory_recfun_params {
    bool m_weaken;
theory_recfun_params() : m_weaken(false) {}
    void updt_params(params_ref const &_p);
    void display(std::ostream &out) const;
};

