namespace plb {
  /*
   * implementation of BoxProcessingFunctional3D_LST
   */
  template<typename T1, template<typename U> class Descriptor, typename T2, typename T3, int nDim>
  void BoxProcessingFunctional3D_LST<T1,Descriptor,T2,T3,nDim>::processGenericBlocks
  (Box3D domain, std::vector<AtomicBlock3D*> atomicBlocks)
  {
    process( domain,
             dynamic_cast<BlockLattice3D<T1,Descriptor>&>(*atomicBlocks[0]),
             dynamic_cast<ScalarField3D<T2>&>(*atomicBlocks[1]),
             dynamic_cast<TensorField3D<T3,nDim>&>(*atomicBlocks[2])
             );
  }

  /* 
   * implementation of KimPeriodicPressureFunctional3D
   */

  template<typename T1, template<typename U> class Descriptor, typename T2, typename T3, int nDim>
  KimPeriodicPressureFunctional3D<T1,Descriptor,T2,T3,nDim>::KimPeriodicPressureFunctional3D
  (T1 rhoTarget_, T1 rhoAvg_, plint dimension_, plint direction_) 
    : rhoTarget(rhoTarget_), rhoAvg(rhoAvg_),
      dimension(dimension_), direction(direction_)
  {
    for(plint iPop=0;iPop<Descriptor<T1>::q;iPop++){
      if(Descriptor<T1>::c[iPop][dimension] == direction){
        rescalePop.push_back(iPop);      
      }
    }
  }
  template<typename T1, template<typename U> class Descriptor, typename T2, typename T3, int nDim>
  void KimPeriodicPressureFunctional3D<T1,Descriptor,T2,T3,nDim>::process
  (Box3D domain, BlockLattice3D<T1,Descriptor>& lattice,
   ScalarField3D<T2> &rho, TensorField3D<T3,nDim> &u)
  {
    // std::cout << global::mpi().getRank() << " "
    //           << domain.x0 << " " << domain.x1 << " | "
    //           << domain.y0 << " " << domain.y1 << " | "
    //           << domain.z0 << " " << domain.z1 << std::endl;
    bool print = global::mpi().getRank() == 3;
    for (plint iX=domain.x0; iX<=domain.x1; ++iX) {
      for (plint iY=domain.y0; iY<=domain.y1; ++iY) {
        for (plint iZ=domain.z0; iZ<=domain.z1; ++iZ) {
          Cell<T1,Descriptor>& cell = lattice.get(iX,iY,iZ);
          T2 rhoPer = rho.get(iX,iY,iZ);
          Array<T3,3> uPer = u.get(iX,iY,iZ);
          
          T2 rhoTargetCell = rhoPer + (rhoTarget - rhoAvg);


          for(IndexVec::iterator it = rescalePop.begin();
              it != rescalePop.end(); it++){
            plint ii = *it;
            cell[ii] -= computeEquilibrium(cell,ii,rhoPer,uPer);
            cell[ii] += computeEquilibrium(cell,ii,rhoTargetCell,uPer);
          }

        }
      }
    }
  }

  template<typename T1, template<typename U> class Descriptor, typename T2, typename T3, int nDim>
  KimPeriodicPressureFunctional3D<T1,Descriptor,T2,T3,nDim>* 
  KimPeriodicPressureFunctional3D<T1,Descriptor,T2,T3,nDim>::clone() const
  { 
    return new KimPeriodicPressureFunctional3D<T1,Descriptor,T2,T3,nDim>(*this);
  }

  template<typename T1, template<typename U> class Descriptor, typename T2, typename T3, int nDim>
  void KimPeriodicPressureFunctional3D<T1,Descriptor,T2,T3,nDim>::getTypeOfModification
  (std::vector<plb::modif::ModifT>& modified) const
  {
    modified[0] = modif::staticVariables;
    modified[1] = modif::nothing;//modif::staticVariables;
    modified[2] = modif::nothing;//modif::staticVariables;
  }

  template<typename T1, template<typename U> class Descriptor, typename T2, typename T3, int nDim>
  T1 KimPeriodicPressureFunctional3D<T1,Descriptor,T2,T3,nDim>::computeEquilibrium(Cell<T1,Descriptor>& cell,
                                                                                   plint iEq,
                                                                                   T2 rho,
                                                                                   Array<T3,Descriptor<T1>::d> u)
  {
    T1 jSqr = 0;
    for(plint i=0;i<Descriptor<T1>::d;i++){
      u[i] *= rho;
      jSqr += u[i]*u[i]; // u now holds j (momentum) but no additional variable needs to be declared
    }
    return cell.computeEquilibrium(iEq,rho,u,jSqr);
  }   
  
  /* 
   * implementation of PeriodicPressureManager
   */

  template<typename T, template<typename U> class Descriptor>
  PeriodicPressureManager<T,Descriptor>::PeriodicPressureManager(MultiBlockLattice3D<T,Descriptor> &lattice, 
                                                                 T rhoIn_, T rhoOut_, Box3D inlet_, Box3D outlet_, 
                                                                 plint dimension_, plint inDirection_,
                                                                 plint outDirection_) 
    : rhoIn(rhoIn_), rhoOut(rhoOut_), rhoAvgIn(0.), rhoAvgOut(0.),
      inlet(inlet_), outlet(outlet_), tmp(inlet_),
      dimension(dimension_), inDirection(inDirection_), outDirection(outDirection_),
    rho(lattice.getNx(),lattice.getNy(),lattice.getNz()), 
    u(lattice.getNx(),lattice.getNy(),lattice.getNz())
  {
    switch(dimension){
    case 0:
      tmp = Box3D(1,1,inlet.y0,inlet.y1,inlet.z0,inlet.z1);
      break;
    case 1:
      tmp = Box3D(inlet.x0,inlet.x1,1,1,inlet.z0,inlet.z1);
      break;
    case 2:
      tmp = Box3D(inlet.x0,inlet.x1,inlet.y0,inlet.y1,1,1);
      break;
    }
  }

  template<typename T, template<typename U> class Descriptor>
  void PeriodicPressureManager<T,Descriptor>::preColl(MultiBlockLattice3D<T,Descriptor> &lattice)
  {
    computeDensity(lattice,rho,inlet); computeDensity(lattice,rho,outlet);
    computeVelocity(lattice,u,inlet); computeVelocity(lattice,u,outlet);
    
    copy(rho,inlet,rho,tmp); copy(rho,outlet,rho,inlet); copy(rho,tmp,rho,outlet);
    copy(u,inlet,u,tmp); copy(u,outlet,u,inlet); copy(u,tmp,u,outlet);
    
    rhoAvgIn = computeAverageDensity(lattice,inlet);
    rhoAvgOut = computeAverageDensity(lattice,outlet);
  }

  template<typename T, template<typename U> class Descriptor>
  void PeriodicPressureManager<T,Descriptor>::postColl(MultiBlockLattice3D<T,Descriptor> &lattice)
  {
    std::vector<MultiBlock3D*> vecIn, vecOut;
    vecIn.push_back(&lattice); vecIn.push_back(&rho); vecIn.push_back(&u);
    vecOut.push_back(&lattice); vecOut.push_back(&rho); vecOut.push_back(&u);

    applyProcessingFunctional
      (new KimPeriodicPressureFunctional3D<T,Descriptor,T,T,Descriptor<T>::d>(rhoIn, 
                                                                              rhoAvgOut,dimension,inDirection),
       inlet,vecIn);
    applyProcessingFunctional
      (new KimPeriodicPressureFunctional3D<T,Descriptor,T,T,Descriptor<T>::d>(rhoOut, 
                                                                              rhoAvgIn,dimension,outDirection),
       outlet,vecOut);
    
  }

  /* 
   * implementation of PressureGradient
   */
  template<typename T>
  PressureGradient<T>::PressureGradient(T pHi_, T pLo_, plint n_, plint dimension_) 
    : pHi(pHi_), pLo(pLo_), n(n_), dimension(dimension_)
  { }
  template<typename T>
  void PressureGradient<T>::operator() (plint iX, plint iY, plint iZ, T& density, Array<T,3>& velocity) const
  {
    velocity.resetToZero();
    switch(dimension){
    case 0:
      density = pHi - (pHi-pLo)*(T)iX/(T)(n-1);
      break;
    case 1:
      density = pHi - (pHi-pLo)*(T)iY/(T)(n-1);
      break;
    case 2:
      density = pHi - (pHi-pLo)*(T)iZ/(T)(n-1);
      break;
    }
  }

};
