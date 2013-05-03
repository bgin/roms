      SUBROUTINE ana_nudgcoef (ng, tile, model)
!
!! svn $Id$
!!================================================= Hernan G. Arango ===
!! Copyright (c) 2002-2013 The ROMS/TOMS Group                         !
!!   Licensed under a MIT/X style license                              !
!!   See License_ROMS.txt                                              !
!=======================================================================
!                                                                      !
!  This routine set nudging coefficients time-scales (1/s).            !
!                                                                      !
!=======================================================================
!
      USE mod_param
      USE mod_ncparam
!
!  Imported variable declarations.
!
      integer, intent(in) :: ng, tile, model
!
!  Local variable declarations.
!
#include "tile.h"
!
      CALL ana_nudgcoef_tile (ng, tile, model,                          &
     &                        LBi, UBi, LBj, UBj,                       &
     &                        IminS, ImaxS, JminS, JmaxS)
!
! Set analytical header file name used.
!
#ifdef DISTRIBUTE
      IF (Lanafile) THEN
#else
      IF (Lanafile.and.(tile.eq.0)) THEN
#endif
        ANANAME(16)=__FILE__
      END IF

      RETURN
      END SUBROUTINE ana_nudgcoef
!
!***********************************************************************
      SUBROUTINE ana_nudgcoef_tile (ng, tile, model,                    &
     &                              LBi, UBi, LBj, UBj,                 &
     &                              IminS, ImaxS, JminS, JmaxS)
!***********************************************************************
!
      USE mod_param
      USE mod_parallel
      USE mod_boundary
#ifdef CLIMATOLOGY
      USE mod_clima
#endif
      USE mod_grid
      USE mod_ncparam
      USE mod_scalars
#ifdef DISTRIBUTE
      USE distribute_mod, ONLY : mp_collect
      USE mp_exchange_mod, ONLY : mp_exchange2d
# ifdef SOLVE3D
      USE mp_exchange_mod, ONLY : mp_exchange3d
# endif
#endif
!
      implicit none
!
!  Imported variable declarations.
!
      integer, intent(in) :: ng, tile, model
      integer, intent(in) :: LBi, UBi, LBj, UBj
      integer, intent(in) :: IminS, ImaxS, JminS, JmaxS
!
!  Local variable declarations.
!
      integer :: Iwrk, i, itrc, j

      real(r8) :: cff1, cff2, cff3

      real(r8), parameter :: IniVal = 0.0_r8

      real(r8), dimension(IminS:ImaxS,JminS:JmaxS) :: wrk

#include "set_bounds.h"
!
!-----------------------------------------------------------------------
!  Set up nudging towards data time-scale coefficients (1/s).
!-----------------------------------------------------------------------
!
!  Initialize.
!
      DO j=JstrT,JendT
        DO i=IstrT,IendT
          wrk(i,j)=0.0_r8
        END DO
      END DO

#if defined CHUKCHI
!
!  nudging coefficients vary from a thirty
!  days time scale at the boundary point to decrease linearly to 0 days
!  (i.e no nudging) 15 grids points away from the boundary.
!
      cff1=1.0_r8/(30_r8*86400.0_r8)          ! 30-day outer limit
      cff2=0.0_r8                             ! Inf days (no nudge) inner limit
      cff3=20.0_r8                            !   width of layer in grid points

! cff3-point wide linearly tapered nudging zone
      DO j=JstrT,MIN(INT(cff3),JendT)               ! SOUTH boundary
        DO i=IstrT,IendT
          wrk(i,j)=cff2+(cff3-REAL(j,r8))*(cff1-cff2)/cff3
        END DO
      END DO
! cff3-point wide linearly tapered nudging zone
      DO j=MAX(JstrT,Mm(ng)+1-INT(cff3)),JendT      ! NORTH boundary
        DO i=IstrT,IendT
          wrk(i,j)=MAX(wrk(i,j),                                        &
     &             cff1+REAL(Mm(ng)+1-j,r8)*(cff2-cff1)/cff3)
        END DO
      END DO
! cff3-point wide linearly tapered nudging zone
      DO i=IstrT,MIN(INT(cff3),IendT)                ! WEST boundary
        DO j=JstrT,JendT
          wrk(i,j)=MAX(wrk(i,j),                                        &
     &             cff2+(cff3-REAL(i,r8))*(cff1-cff2)/cff3)
        END DO
      END DO
! cff3-point wide linearly tapered nudging zone
      DO i=MAX(IstrT,Lm(ng)+1-INT(cff3)),IendT       ! EAST boundary
        DO j=MAX(400,JstrT),JendT
          wrk(i,j)=MAX(wrk(i,j),                                        &
     &             cff1+REAL(Lm(ng)+1-i,r8)*(cff2-cff1)/cff3)
        END DO
      END DO
!
! Set the relevant nudging coefficients using the entries in wrk
!
# ifdef ZCLM_NUDGING
      DO j=JstrT,JendT
        DO i=IstrT,IendT
          CLIMA(ng)%Znudgcof(i,j)=wrk(i,j)
        END DO
      END DO
# endif
# ifdef M2CLM_NUDGING
      DO j=JstrT,JendT
        DO i=IstrT,IendT
          CLIMA(ng)%M2nudgcof(i,j)=wrk(i,j)
        END DO
      END DO
# endif
# ifdef SOLVE3D
#  ifdef TCLM_NUDGING
      DO j=JstrT,JendT
        DO i=IstrT,IendT
          CLIMA(ng)%Tnudgcof(i,j,itemp)=wrk(i,j)
          CLIMA(ng)%Tnudgcof(i,j,isalt)=wrk(i,j)
        END DO
      END DO
#  endif
#  ifdef M3CLM_NUDGING
      DO j=JstrT,JendT
        DO i=IstrT,IendT
          CLIMA(ng)%M3nudgcof(i,j)=wrk(i,j)
        END DO
      END DO
#  endif
#  ifdef AICLM_NUDGING
! Set these ice timescales to a day at the edge, not 30.
      DO j=JstrT,JendT
        DO i=IstrT,IendT
          CLIMA(ng)%AInudgcof(i,j)=wrk(i,j) * 30.
        END DO
      END DO
#  endif
#  ifdef MICLM_NUDGING
      DO j=JstrT,JendT
        DO i=IstrT,IendT
          CLIMA(ng)%MInudgcof(i,j)=wrk(i,j) * 30.
        END DO
      END DO
#  endif
# endif
#else
!
!  Default nudging coefficients.  Set nudging coefficients uniformly to
!  the values specified in the standard input file.
!
# ifdef ZCLM_NUDGING
      DO j=JstrT,JendT
        DO i=IstrT,IendT
          CLIMA(ng)%Znudgcof(i,j)=Znudg(ng)
        END DO
      END DO
# endif
# ifdef M2CLM_NUDGING
      DO j=JstrT,JendT
        DO i=IstrT,IendT
          CLIMA(ng)%M2nudgcof(i,j)=M2nudg(ng)
        END DO
      END DO
# endif
# ifdef SOLVE3D
#  ifdef TCLM_NUDGING
      DO itrc=1,NT(ng)
        DO j=JstrT,JendT
          DO i=IstrT,IendT
            CLIMA(ng)%Tnudgcof(i,j,itrc)=Tnudg(itrc,ng)
          END DO
        END DO
      END DO
#  endif
#  ifdef M3CLM_NUDGING
      DO j=JstrT,JendT
        DO i=IstrT,IendT
          CLIMA(ng)%M3nudgcof(i,j)=M3nudg(ng)
        END DO
      END DO
#  endif
# endif
#endif
!
!-----------------------------------------------------------------------
!  Set nudging coefficients (1/s) for passive/active (outflow/inflow)
!  open boundary conditions.  Weak nudging is expected in passive
!  outflow conditions and strong nudging is expected in active inflow
!  conditions.  Notice that interior nudging coefficient defined
!  above are zero out when boundary condition nudging.  The USER needs
!  to adapt this to his/her application!
!-----------------------------------------------------------------------
!
!! WARNING:  This section is generic for all applications. Please do not
!!           change the code below.
!!
!  Free-surface nudging coefficients.
!
# ifdef WEST_FSNUDGING
#  ifdef ZCLM_NUDGING
      IF (DOMAIN(ng)%SouthWest_Corner(tile)) THEN
        FSobc_out(ng,iwest)=Znudgcof(ng)
        FSobc_in (ng,iwest)=obcfac(ng)*FSobc_out(ng,iwest)
      END IF
      IF (DOMAIN(ng)%Western_Edge(tile)) THEN
        DO j=JstrT,JendT
          CLIMA(ng)%Znudgcof(0,j)=0.0_r8
        END DO
      END IF
#   ifdef DISTRIBUTE
      IF (ng.eq.Ngrids) THEN
        CALL mp_collect (ng, model, Ngrids, IniVal, FSobc_out(:,iwest))
        CALL mp_collect (ng, model, Ngrids, IniVal, FSobc_in (:,iwest))
      END IF
#   endif
#  else
      IF (DOMAIN(ng)%SouthWest_Test(tile)) THEN
        FSobc_out(ng,iwest)=Znudg(ng)
        FSobc_in (ng,iwest)=obcfac(ng)*Znudg(ng)
      END IF
#  endif
# endif
# ifdef EAST_FSNUDGING
#  ifdef ZCLM_NUDGING
      IF (DOMAIN(ng)%NorthEast_Corner(tile)) THEN
        FSobc_out(ng,ieast)=Znudg(ng)
        FSobc_in (ng,ieast)=obcfac(ng)*FSobc_out(ng,ieast)
      END IF
      IF (DOMAIN(ng)%Eastern_Edge(tile)) THEN
        DO j=JstrT,JendT
          CLIMA(ng)%Znudgcof(Lm(ng)+1,j)=0.0_r8
        END DO
      END IF
#   ifdef DISTRIBUTE
      IF (ng.eq.Ngrids) THEN
        CALL mp_collect (ng, model, Ngrids, IniVal, FSobc_out(:,ieast))
        CALL mp_collect (ng, model, Ngrids, IniVal, FSobc_in (:,ieast))
      END IF
#   endif
#  else
      IF (DOMAIN(ng)%NorthEast_Test(tile)) THEN
        FSobc_out(ng,ieast)=Znudg(ng)
        FSobc_in (ng,ieast)=obcfac(ng)*Znudg(ng)
      END IF
#  endif
# endif
# ifdef SOUTH_FSNUDGING
#  ifdef ZCLM_NUDGING
      IF (DOMAIN(ng)%SouthWest_Corner(tile)) THEN
        FSobc_out(ng,isouth)=Znudgcof(ng)
        FSobc_in (ng,isouth)=obcfac(ng)*FSobc_out(ng,isouth)
      END IF
      IF (DOMAIN(ng)%Southern_Edge(tile)) THEN
        DO i=IstrT,IendT
          CLIMA(ng)%Znudgcof(i,0)=0.0_r8
        END DO
      END IF
#   ifdef DISTRIBUTE
      IF (ng.eq.Ngrids) THEN
        CALL mp_collect (ng, model, Ngrids, IniVal, FSobc_out(:,isouth))
        CALL mp_collect (ng, model, Ngrids, IniVal, FSobc_in (:,isouth))
      END IF
#   endif
#  else
      IF (DOMAIN(ng)%SouthWest_Test(tile)) THEN
        FSobc_out(ng,isouth)=Znudg(ng)
        FSobc_in (ng,isouth)=obcfac(ng)*Znudg(ng)
      END IF
#  endif
# endif
# ifdef NORTH_FSNUDGING
#  ifdef ZCLM_NUDGING
      IF (DOMAIN(ng)%NorthEast_Corner(tile)) THEN
        FSobc_out(ng,inorth)=Znudg(ng)
        FSobc_in (ng,inorth)=obcfac(ng)*FSobc_out(ng,inorth)
      END IF
      IF (DOMAIN(ng)%Northern_Edge(tile)) THEN
        DO i=IstrT,IendT
          CLIMA(ng)%Znudgcof(i,Mm(ng)+1)=0.0_r8
        END DO
      END IF
#   ifdef DISTRIBUTE
      IF (ng.eq.Ngrids) THEN
        CALL mp_collect (ng, model, Ngrids, IniVal, FSobc_out(:,inorth))
        CALL mp_collect (ng, model, Ngrids, IniVal, FSobc_in (:,inorth))
      END IF
#   endif
#  else
      IF (DOMAIN(ng)%NorthEast_Test(tile)) THEN
        FSobc_out(ng,inorth)=Znudg(ng)
        FSobc_in (ng,inorth)=obcfac(ng)*Znudg(ng)
      END IF
#  endif
# endif
!
!  2D momentum nudging coefficients.
!
# ifdef WEST_M2NUDGING
#  ifdef M2CLM_NUDGING
      IF (DOMAIN(ng)%SouthWest_Corner(tile)) THEN
        M2obc_out(ng,iwest)=M2nudg(ng)
        M2obc_in (ng,iwest)=obcfac(ng)*M2obc_out(ng,iwest)
      END IF
      IF (DOMAIN(ng)%Western_Edge(tile)) THEN
        DO j=JstrT,JendT
          CLIMA(ng)%M2nudgcof(0,j)=-CLIMA(ng)%M2nudgcof(1,j)
        END DO
      END IF
#   ifdef DISTRIBUTE
      IF (ng.eq.Ngrids) THEN
        CALL mp_collect (ng, model, Ngrids, IniVal, M2obc_out(:,iwest))
        CALL mp_collect (ng, model, Ngrids, IniVal, M2obc_in (:,iwest))
      END IF
#   endif
#  else
      IF (DOMAIN(ng)%SouthWest_Test(tile)) THEN
        M2obc_out(ng,iwest)=M2nudg(ng)
        M2obc_in (ng,iwest)=obcfac(ng)*M2nudg(ng)
      END IF
#  endif
# endif
# ifdef EAST_M2NUDGING
#  ifdef M2CLM_NUDGING
      IF (DOMAIN(ng)%NorthEast_Corner(tile)) THEN
        M2obc_out(ng,ieast)=M2nudg(ng)
        M2obc_in (ng,ieast)=obcfac(ng)*M2obc_out(ng,ieast)
      END IF
      IF (DOMAIN(ng)%Eastern_Edge(tile)) THEN
        DO j=JstrT,JendT
          CLIMA(ng)%M2nudgcof(Lm(ng)+1,j)=-CLIMA(ng)%M2nudgcof(Lm(ng),j)
        END DO
      END IF
#   ifdef DISTRIBUTE
      IF (ng.eq.Ngrids) THEN
        CALL mp_collect (ng, model, Ngrids, IniVal, M2obc_out(:,ieast))
        CALL mp_collect (ng, model, Ngrids, IniVal, M2obc_in (:,ieast))
      END IF
#   endif
#  else
      IF (DOMAIN(ng)%NorthEast_Test(tile)) THEN
        M2obc_out(ng,ieast)=M2nudg(ng)
        M2obc_in (ng,ieast)=obcfac(ng)*M2nudg(ng)
      END IF
#  endif
# endif
# ifdef SOUTH_M2NUDGING
#  ifdef M2CLM_NUDGING
      IF (DOMAIN(ng)%SouthWest_Corner(tile)) THEN
        M2obc_out(ng,isouth)=M2nudg(ng)
        M2obc_in (ng,isouth)=obcfac(ng)*M2obc_out(ng,isouth)
      END IF
      IF (DOMAIN(ng)%Southern_Edge(tile)) THEN
        DO i=IstrT,IendT
          CLIMA(ng)%M2nudgcof(i,0)=-CLIMA(ng)%M2nudgcof(i,1)
        END DO
      END IF
#   ifdef DISTRIBUTE
      IF (ng.eq.Ngrids) THEN
        CALL mp_collect (ng, model, Ngrids, IniVal, M2obc_out(:,isouth))
        CALL mp_collect (ng, model, Ngrids, IniVal, M2obc_in (:,isouth))
      END IF
#   endif
#  else
      IF (DOMAIN(ng)%SouthWest_Test(tile)) THEN
        M2obc_out(ng,isouth)=M2nudg(ng)
        M2obc_in (ng,isouth)=obcfac(ng)*M2nudg(ng)
      END IF
#  endif
# endif
# ifdef NORTH_M2NUDGING
#  ifdef M2CLM_NUDGING
      IF (DOMAIN(ng)%NorthEast_Corner(tile)) THEN
        M2obc_out(ng,inorth)=M2nudg(ng)
        M2obc_in (ng,inorth)=obcfac(ng)*M2obc_out(ng,inorth)
      END IF
      IF (DOMAIN(ng)%Northern_Edge(tile)) THEN
        DO i=IstrT,IendT
          CLIMA(ng)%M2nudgcof(i,Mm(ng)+1)=-CLIMA(ng)%M2nudgcof(i,Mm(ng))
        END DO
      END IF
#   ifdef DISTRIBUTE
      IF (ng.eq.Ngrids) THEN
        CALL mp_collect (ng, model, Ngrids, IniVal, M2obc_out(:,inorth))
        CALL mp_collect (ng, model, Ngrids, IniVal, M2obc_in (:,inorth))
      END IF
#   endif
#  else
      IF (DOMAIN(ng)%NorthEast_Test(tile)) THEN
        M2obc_out(ng,inorth)=M2nudg(ng)
        M2obc_in (ng,inorth)=obcfac(ng)*M2nudg(ng)
      END IF
#  endif
# endif
# ifdef SOLVE3D
!
!  Tracers nudging coefficients.
!
#  ifdef WEST_TNUDGING
#   ifdef TCLM_NUDGING
      DO itrc=1,NT(ng)
        IF (DOMAIN(ng)%SouthWest_Corner(tile)) THEN
          Tobc_out(itrc,ng,iwest)=Tnudg(itrc,ng)
          Tobc_in (itrc,ng,iwest)=obcfac(ng)*Tobc_out(itrc,ng,iwest)
        END IF
        IF (DOMAIN(ng)%Western_Edge(tile)) THEN
          DO j=JstrT,JendT
            CLIMA(ng)%Tnudgcof(0,j,itrc)=0.0_r8
          END DO
        END IF
      END DO
#    ifdef DISTRIBUTE
      CALL mp_collect (ng, model, MT, IniVal, Tobc_out(:,ng,iwest))
      CALL mp_collect (ng, model, MT, IniVal, Tobc_in (:,ng,iwest))
#    endif
#   else
      DO itrc=1,NT(ng)
        IF (DOMAIN(ng)%SouthWest_Test(tile)) THEN
          Tobc_out(itrc,ng,iwest)=Tnudg(itrc,ng)
          Tobc_in (itrc,ng,iwest)=obcfac(ng)*Tnudg(itrc,ng)
        END IF
      END DO
#   endif
#  endif
#  ifdef EAST_TNUDGING
#   ifdef TCLM_NUDGING
      DO itrc=1,NT(ng)
        IF (DOMAIN(ng)%NorthEast_Corner(tile)) THEN
          Tobc_out(itrc,ng,ieast)=Tnudg(itrc,ng)
          Tobc_in (itrc,ng,ieast)=obcfac(ng)*Tobc_out(itrc,ng,ieast)
        END IF
        IF (DOMAIN(ng)%Eastern_Edge(tile)) THEN
          DO j=JstrT,JendT
            CLIMA(ng)%Tnudgcof(Lm(ng)+1,j,itrc)=0.0_r8
          END DO
        END IF
      END DO
#    ifdef DISTRIBUTE
      CALL mp_collect (ng, model, MT, IniVal, Tobc_out(:,ng,ieast))
      CALL mp_collect (ng, model, MT, IniVal, Tobc_in (:,ng,ieast))
#    endif
#   else
      DO itrc=1,NT(ng)
        IF (DOMAIN(ng)%NorthEast_Test(tile)) THEN
          Tobc_out(itrc,ng,ieast)=Tnudg(itrc,ng)
          Tobc_in (itrc,ng,ieast)=obcfac(ng)*Tnudg(itrc,ng)
        END IF
      END DO
#   endif
#  endif
#  ifdef SOUTH_TNUDGING
#   ifdef TCLM_NUDGING
      DO itrc=1,NT(ng)
        IF (DOMAIN(ng)%SouthWest_Corner(tile)) THEN
          Tobc_out(itrc,ng,isouth)=Tnudg(itrc,ng)
          Tobc_in (itrc,ng,isouth)=obcfac(ng)*Tobc_out(itrc,ng,isouth)
        END IF
        IF (DOMAIN(ng)%Southern_Edge(tile)) THEN
          DO i=IstrT,IendT
            CLIMA(ng)%Tnudgcof(i,0,itrc)=0.0_r8
          END DO
        END IF
      END DO
#    ifdef DISTRIBUTE
      CALL mp_collect (ng, model, MT, IniVal, Tobc_out(:,ng,isouth))
      CALL mp_collect (ng, model, MT, IniVal, Tobc_in (:,ng,isouth))
#    endif
#   else
      DO itrc=1,NT(ng)
        IF (DOMAIN(ng)%SouthWest_Test(tile)) THEN
          Tobc_out(itrc,ng,isouth)=Tnudg(itrc,ng)
          Tobc_in (itrc,ng,isouth)=obcfac(ng)*Tnudg(itrc,ng)
        END IF
      END DO
#   endif
#  endif
#  ifdef NORTH_TNUDGING
#   ifdef TCLM_NUDGING
      DO itrc=1,NT(ng)
        IF (DOMAIN(ng)%NorthEast_Corner(tile)) THEN
          Tobc_out(itrc,ng,inorth)=Tnudg(itrc,ng)
          Tobc_in (itrc,ng,inorth)=obcfac(ng)*Tobc_out(itrc,ng,inorth)
        END IF
        IF (DOMAIN(ng)%Northern_Edge(tile)) THEN
          DO i=IstrT,IendT
            CLIMA(ng)%Tnudgcof(i,Mm(ng)+1,itrc)=0.0_r8
          END DO
        END IF
      END DO
#    ifdef DISTRIBUTE
      CALL mp_collect (ng, model, MT, IniVal, Tobc_out(:,ng,inorth))
      CALL mp_collect (ng, model, MT, IniVal, Tobc_in (:,ng,inorth))
#    endif
#   else
      DO itrc=1,NT(ng)
        IF (DOMAIN(ng)%NorthEast_Test(tile)) THEN
          Tobc_out(itrc,ng,inorth)=Tnudg(itrc,ng)
          Tobc_in (itrc,ng,inorth)=obcfac(ng)*Tnudg(itrc,ng)
        END IF
      END DO
#   endif
#  endif
!
!  3D momentum nudging coefficients.
!
#  ifdef WEST_M3NUDGING
#   ifdef M3CLM_NUDGING
      IF (DOMAIN(ng)%SouthWest_Corner(tile)) THEN
        M3obc_out(ng,iwest)=M3nudg(ng)
        M3obc_in (ng,iwest)=obcfac(ng)*M3obc_out(ng,iwest)
      END IF
      IF (DOMAIN(ng)%Western_Edge(tile)) THEN
        DO j=JstrT,JendT
          CLIMA(ng)%M3nudgcof(0,j)=-CLIMA(ng)%M3nudgcof(1,j)
        END DO
      END IF
#    ifdef DISTRIBUTE
      IF (ng.eq.Ngrids) THEN
        CALL mp_collect (ng, model, Ngrids, IniVal, M3obc_out(:,iwest))
        CALL mp_collect (ng, model, Ngrids, IniVal, M3obc_in (:,iwest))
      END IF
#    endif
#   else
      IF (DOMAIN(ng)%SouthWest_Test(tile)) THEN
        M3obc_out(ng,iwest)=M3nudg(ng)
        M3obc_in (ng,iwest)=obcfac(ng)*M3nudg(ng)
      END IF
#   endif
#  endif
#  ifdef EAST_M3NUDGING
#   ifdef M3CLM_NUDGING
      IF (DOMAIN(ng)%NorthEast_Corner(tile)) THEN
        M3obc_out(ng,ieast)=M3nudg(ng)
        M3obc_in (ng,ieast)=obcfac(ng)*M3obc_out(ng,ieast)
      END IF
      IF (DOMAIN(ng)%Eastern_Edge(tile)) THEN
        DO j=JstrT,JendT
          CLIMA(ng)%M3nudgcof(Lm(ng)+1,j)=-CLIMA(ng)%M3nudgcof(Lm(ng),j)
        END DO
      END IF
#    ifdef DISTRIBUTE
      IF (ng.eq.Ngrids) THEN
        CALL mp_collect (ng, model, Ngrids, IniVal, M3obc_out(:,ieast))
        CALL mp_collect (ng, model, Ngrids, IniVal, M3obc_in (:,ieast))
      END IF
#    endif
#   else
      IF (DOMAIN(ng)%NorthEast_Test(tile)) THEN
        M3obc_out(ng,ieast)=M3nudg(ng)
        M3obc_in (ng,ieast)=obcfac(ng)*M3nudg(ng)
      END IF
#   endif
#  endif
#  ifdef SOUTH_M3NUDGING
#   ifdef M3CLM_NUDGING
      IF (DOMAIN(ng)%SouthWest_Corner(tile)) THEN
        M3obc_out(ng,isouth)=M3nudg(ng)
        M3obc_in (ng,isouth)=obcfac(ng)*M3obc_out(ng,isouth)
      END IF
      IF (DOMAIN(ng)%Southern_Edge(tile)) THEN
        DO i=IstrT,IendT
          CLIMA(ng)%M3nudgcof(i,0)=-CLIMA(ng)%M3nudgcof(i,1)
        END DO
      END IF
#    ifdef DISTRIBUTE
      IF (ng.eq.Ngrids) THEN
        CALL mp_collect (ng, model, Ngrids, IniVal, M3obc_out(:,isouth))
        CALL mp_collect (ng, model, Ngrids, IniVal, M3obc_in (:,isouth))
      END IF
#    endif
#   else
      IF (DOMAIN(ng)%SouthWest_Test(tile)) THEN
        M3obc_out(ng,isouth)=M3nudg(ng)
        M3obc_in (ng,isouth)=obcfac(ng)*M3nudg(ng)
      END IF
#   endif
#  endif
#  ifdef NORTH_M3NUDGING
#   ifdef M3CLM_NUDGING
      IF (DOMAIN(ng)%NorthEast_Corner(tile)) THEN
        M3obc_out(ng,inorth)=M3nudg(ng)
        M3obc_in (ng,inorth)=obcfac(ng)*M3obc_out(ng,inorth)
      END IF
      IF (DOMAIN(ng)%Northern_Edge(tile)) THEN
        DO i=IstrT,IendT
          CLIMA(ng)%M3nudgcof(i,Mm(ng)+1)=-CLIMA(ng)%M3nudgcof(i,Mm(ng))
        END DO
      END IF
#    ifdef DISTRIBUTE
      IF (ng.eq.Ngrids) THEN
        CALL mp_collect (ng, model, Ngrids, IniVal, M3obc_out(:,inorth))
        CALL mp_collect (ng, model, Ngrids, IniVal, M3obc_in (:,inorth))
      END IF
#    endif
#   else
      IF (DOMAIN(ng)%NorthEast_Test(tile)) THEN
        M3obc_out(ng,inorth)=M3nudg(ng)
        M3obc_in (ng,inorth)=obcfac(ng)*M3nudg(ng)
      END IF
#   endif
#  endif
# endif
#ifdef DISTRIBUTE
# ifdef M2CLM_NUDGING
      CALL mp_exchange2d (ng, tile, model, 1,                           &
     &                    LBi, UBi, LBj, UBj,                           &
     &                    NghostPoints, .FALSE., .FALSE.,               &
     &                    CLIMA(ng)%M2nudgcof)
# endif
# ifdef SOLVE3D
#  ifdef M3CLM_NUDGING
      CALL mp_exchange2d (ng, tile, model, 1,                           &
     &                    LBi, UBi, LBj, UBj,                           &
     &                    NghostPoints, .FALSE., .FALSE.,               &
     &                    CLIMA(ng)%M3nudgcof)
#  endif
#  ifdef TCLM_NUDGING
      CALL mp_exchange3d (ng, tile, model, 1,                           &
     &                    LBi, UBi, LBj, UBj, 1, NT(ng),                &
     &                    NghostPoints, .FALSE., .FALSE.,               &
     &                    CLIMA(ng)%Tnudgcof)
#  endif
# endif
#endif

      RETURN
      END SUBROUTINE ana_nudgcoef_tile
