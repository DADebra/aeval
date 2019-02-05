(declare-fun $~flatten0$5 () Bool)
(declare-fun $V275_X$5 () Bool)
(declare-fun $V61_close_door$5 () Bool)
(declare-fun $V268_door_initially_closed$5 () Bool)
(declare-fun $V265_door_doesnt_close_if_not_asked$5 () Bool)
(declare-fun $V271_warning_start_only_in_station$5 () Bool)
(declare-fun $V266_door_doesnt_open_if_not_asked$5 () Bool)
(declare-fun $V270_warning_start_and_in_station_go_down_simultaneously$5
             ()
             Bool)
(declare-fun $V253_between_A_and_X$5 () Bool)
(declare-fun $V250_door_doesnt_open_out_of_station$5 () Bool)
(declare-fun $V267_tramway_doesnt_start_if_not_door_ok$5 () Bool)
(declare-fun $V264_env_ok$5 () Bool)
(declare-fun $OK$5 () Bool)
(declare-fun $V269_initially_not_in_station$5 () Bool)
(declare-fun $V252_X$2 () Bool)
(declare-fun $~flatten0$2 () Bool)
(declare-fun $V276_X$2 () Bool)
(declare-fun $V273_X$2 () Bool)
(declare-fun $V252_X$5 () Bool)
(declare-fun $V60_open_door$5 () Bool)
(declare-fun $warning_start$3 () Bool)
(declare-fun $V59_prop_ok$5 () Bool)
(declare-fun $V275_X$2 () Bool)
(declare-fun $V276_X$5 () Bool)
(declare-fun $V274_X$5 () Bool)
(declare-fun $V62_door_ok$2 () Bool)
(declare-fun $V272_warning_start_cant_become_true_when_door_is_opening$5
             ()
             Bool)
(declare-fun $request_door$2 () Bool)
(declare-fun $V251_door_opens_before_leaving_station$5 () Bool)
(declare-fun $V253_between_A_and_X$2 () Bool)
(declare-fun $door_is_open$3 () Bool)
(declare-fun $in_station$3 () Bool)
(declare-fun $V274_X$2 () Bool)
(declare-fun $V58_env_always_ok$2 () Bool)
(declare-fun $door_is_open$2 () Bool)
(declare-fun $V273_X$5 () Bool)
(declare-fun $V58_env_always_ok$5 () Bool)
(declare-fun $warning_start$2 () Bool)

(assert (let ((a!1 (or (not (or (not $in_station$3) (not $V274_X$2))) $V62_door_ok$2))
      (a!3 (not (or (and $request_door$2 (not $warning_start$2))
                    (and (not $~flatten0$2) $V253_between_A_and_X$2)
                    (not $in_station$3)
                    (not $V252_X$2))))
      (a!5 (or false (not (or (not $door_is_open$3) (not $V273_X$2)))))
      (a!6 (or $V62_door_ok$2 (not (or (not $in_station$3) (not $V274_X$2)))))
      (a!7 (= (ite false false (or (not $in_station$3) (not $V275_X$2)))
              (ite false false (or (not $warning_start$3) (not $V276_X$2)))))
      (a!8 (or (not (or $warning_start$3 (not $warning_start$2))) (not true)))
      (a!10 (ite (and (not $warning_start$2) $request_door$2)
                 true
                 (ite (ite false false $~flatten0$2)
                      false
                      $V253_between_A_and_X$2))))
(let ((a!2 (and (not (or (not $door_is_open$3) (not $V273_X$2)))
                a!1
                (= (or (not $in_station$3) (not $V275_X$2))
                   (or (not $warning_start$3) (not $V276_X$2)))
                (or (not $warning_start$3) $in_station$3)
                (not (or $warning_start$3 (not $warning_start$2)))
                $V58_env_always_ok$2))
      (a!9 (and true
                true
                true
                a!5
                a!6
                a!7
                (or $in_station$3 (not $warning_start$3))
                a!8))
      (a!11 (or a!10 (ite false false (or (not $in_station$3) (not $V252_X$2))))))
(let ((a!4 (or (not a!2) (and (or (not $door_is_open$3) $in_station$3) a!3)))
      (a!12 (or (not (and $V58_env_always_ok$2 a!9))
                (and (or $in_station$3 (not $door_is_open$3)) (not a!11))))
      (a!13 (= $V59_prop_ok$5
               (and (or $in_station$3 (not $door_is_open$3)) (not a!11)))))
(let ((a!14 (and (= $OK$5 a!12)
                 (= $V58_env_always_ok$5 (and $V58_env_always_ok$2 a!9))
                 a!13
                 (= $V264_env_ok$5 a!9)
                 (= $V250_door_doesnt_open_out_of_station$5
                    (or $in_station$3 (not $door_is_open$3)))
                 (= $V251_door_opens_before_leaving_station$5 (not a!11))
                 (= $V253_between_A_and_X$5 a!10)
                 (= $V252_X$5 (not $in_station$3))
                 (= $V266_door_doesnt_open_if_not_asked$5 a!5)
                 (= $V265_door_doesnt_close_if_not_asked$5 true)
                 (= $V267_tramway_doesnt_start_if_not_door_ok$5 a!6)
                 (= $V268_door_initially_closed$5 true)
                 (= $V269_initially_not_in_station$5 true)
                 (= $V270_warning_start_and_in_station_go_down_simultaneously$5
                    a!7)
                 (= $V271_warning_start_only_in_station$5
                    (or $in_station$3 (not $warning_start$3)))
                 (= $V272_warning_start_cant_become_true_when_door_is_opening$5
                    a!8)
                 (= $V60_open_door$5 true)
                 (= $V273_X$5 (not $door_is_open$3))
                 (= $V61_close_door$5 false)
                 (= $V274_X$5 (not $in_station$3))
                 (= $V275_X$5 (not $in_station$3))
                 (= $V276_X$5 (not $warning_start$3))
                 (= $~flatten0$5 $door_is_open$2))))
  (ite a!4 a!14 true))))))
(check-sat)