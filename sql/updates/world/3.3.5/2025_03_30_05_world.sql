-- Dazalar outro emote when "Taming the Beast" (6102) is rewarded.
UPDATE `creature_template` SET `AIName`="SmartAI" WHERE `entry`=3601;
DELETE FROM `smart_scripts` WHERE `entryorguid`=3601;
DELETE FROM `smart_scripts` WHERE `entryorguid`=360100 AND `source_type`=9;
INSERT INTO `smart_scripts` (`entryorguid`,`source_type`,`id`,`link`,`event_type`,`event_phase_mask`,`event_chance`,`event_flags`,`event_param1`,`event_param2`,`event_param3`,`event_param4`,`event_param5`,`action_type`,`action_param1`,`action_param2`,`action_param3`,`action_param4`,`action_param5`,`action_param6`,`target_type`,`target_param1`,`target_param2`,`target_param3`,`target_param4`,`target_x`,`target_y`,`target_z`,`target_o`,`comment`) VALUES
(3601,0,0,0,20,0,100,0,6102,0,0,0,0,80,360100,0,0,0,0,0,1,0,0,0,0,0,0,0,0,"Dazalar - On Quest 'Taming the Beast' Rewarded - Run Script"),
(360100,9,0,0,0,0,100,0,0,0,0,0,0,66,0,0,0,0,0,0,7,0,0,0,0,0,0,0,0,"Dazalar - On Script - Set Orientation Invoker"),
(360100,9,1,0,0,0,100,0,0,0,0,0,0,5,469,0,0,0,0,0,1,0,0,0,0,0,0,0,0,"Dazalar - On Script - Play Emote"),
(360100,9,2,0,0,0,100,0,2000,2000,0,0,0,66,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,"Dazalar - On Script - Set Orientation Home Position");
