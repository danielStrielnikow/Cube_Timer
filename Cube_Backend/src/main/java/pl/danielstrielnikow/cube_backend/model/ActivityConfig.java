package pl.danielstrielnikow.cube_backend.model;

import jakarta.persistence.*;
import lombok.Data;

@Entity
@Table(name = "activity_configs")
@Data
public class ActivityConfig {

    @Id
    @GeneratedValue(strategy = GenerationType.IDENTITY)
    private Long id;

    @ManyToOne
    private Cube cube;

    private Integer sideNumber; // 1-6
}
