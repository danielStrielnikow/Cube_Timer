package pl.danielstrielnikow.cube_backend.model.dto;

import lombok.Data;

@Data
public class CubeEventDTO {
    private String cubeId;    // Np. "CUBE-01"
    private Integer side;     // 1-6
    private Double battery;   // Np. 3.85
}
